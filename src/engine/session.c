#include "session.h"

#include <math.h>
#include <string.h>

static void deploy(FodderSession *session)
{
    const FodderVariant *variant = &session->pack->variants[session->profile.variant];
    FodderEffects effects = fodder_pack_effects(session->pack, session->profile.rank);
    fodder_battle_start(&session->battle, &session->pack->arena, variant, effects,
                        fodder_profile_seed(&session->profile), FODDER_ARMY_SIZE);
    session->time_buffer = 0.0;
    fodder_session_clear_input(session);
}

void fodder_session_open(FodderSession *session, const FodderPack *pack, FodderProfile profile,
                          FodderSave save, void *storage)
{
    memset(session, 0, sizeof(*session));
    session->pack = pack;
    session->profile = profile;
    session->save = save;
    session->storage = storage;
    session->screen = profile.pending ? FODDER_REWARD : FODDER_CAMP;
    deploy(session);
}

void fodder_session_saved(FodderSession *session, bool success)
{
    if (!session->saving)
        return;
    session->saving = false;
    session->save_error = !success;
    if (!success)
        return;

    session->profile = session->pending_profile;
    session->screen = session->after_save;
    session->selection = 0;
    if (session->screen == FODDER_CAMP)
        deploy(session);
}

static void save_profile(FodderSession *session)
{
    session->saving = true;
    session->save_error = false;
    FodderWrite result = session->save(session->storage, &session->profile, &session->pending_profile);
    if (result != FODDER_WRITE_PENDING)
        fodder_session_saved(session, result == FODDER_WRITE_COMPLETE);
}

static void claim(FodderSession *session, unsigned upgrade)
{
    if (fodder_profile_claim(&session->pending_profile, &session->profile,
                             upgrade, session->pack->variant_count)) {
        session->after_save = FODDER_CAMP;
        save_profile(session);
    }
}

void fodder_session_clear_input(FodderSession *session)
{
    session->queued_x = 0.0f;
    session->queued_y = 0.0f;
    session->attack_down = false;
    session->queued_attack = false;
    session->battle.attack_held = false;
    session->battle.block_held = false;
    session->battle.guard_window = 0.0f;
}

void fodder_session_action(FodderSession *session, FodderAction action)
{
    if (session->saving)
        return;
    if (session->save_error) {
        if (action == FODDER_CONFIRM)
            save_profile(session);
        return;
    }
    fodder_session_clear_input(session);
    switch (session->screen) {
    case FODDER_CAMP:
    case FODDER_RESULT:
        if (action == FODDER_CONFIRM) {
            deploy(session);
            session->screen = FODDER_FIGHT;
        } else if (action == FODDER_BACK) {
            session->screen = FODDER_CAMP;
        }
        break;
    case FODDER_FIGHT:
        if (action == FODDER_BACK)
            session->screen = FODDER_PAUSED;
        break;
    case FODDER_PAUSED:
        if (action == FODDER_CONFIRM || action == FODDER_BACK)
            session->screen = FODDER_FIGHT;
        break;
    case FODDER_REWARD:
        if (action == FODDER_PREVIOUS || action == FODDER_NEXT) {
            unsigned delta = action == FODDER_PREVIOUS ? FODDER_UPGRADES - 1 : 1;
            session->selection = (session->selection + delta) % FODDER_UPGRADES;
        } else if (action == FODDER_CONFIRM) {
            claim(session, session->selection);
        } else if (action >= FODDER_FIRST_UPGRADE && action <= FODDER_THIRD_UPGRADE) {
            claim(session, (unsigned)(action - FODDER_FIRST_UPGRADE));
        }
        break;
    }
}

void fodder_session_frame(FodderSession *session, double elapsed, FodderInput input)
{
    static const double tick_time = 1.0 / FODDER_TICKS_PER_SECOND;
    if (session->screen != FODDER_FIGHT || session->saving || session->save_error)
        return;
    session->time_buffer += fmin(0.15, fmax(elapsed, 0.0));
    session->queued_x += input.look_x;
    session->queued_y += input.look_y;
    session->queued_attack |= input.attack_pressed || (input.attack && !session->attack_down);
    session->attack_down = input.attack;
    while (session->time_buffer >= tick_time) {
        session->time_buffer -= tick_time;
        input.look_x = session->queued_x;
        input.look_y = session->queued_y;
        input.attack_pressed = session->queued_attack;
        session->queued_x = session->queued_y = 0.0f;
        session->queued_attack = false;
        fodder_battle_tick(&session->battle, input);
        if (session->battle.outcome != FODDER_ACTIVE) {
            if (fodder_profile_result(&session->pending_profile, &session->profile,
                                      session->battle.outcome, session->battle.kills, session->battle.deeds)) {
                session->after_save = session->battle.outcome == FODDER_SURVIVED ?
                                      FODDER_REWARD : FODDER_RESULT;
                save_profile(session);
            }
            break;
        }
    }
}
