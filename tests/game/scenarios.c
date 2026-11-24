#include "session.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define EXPECT(test) do { if (!(test)) { fprintf(stderr, "Game scenario failed at %d: %s\n", __LINE__, #test); exit(1); } } while (0)

typedef struct {
    char saved[FODDER_PROFILE_TEXT];
    unsigned commits;
} Save;

static FodderWrite record(void *data, const FodderProfile *before, const FodderProfile *after)
{
    Save *save = data;
    if (before->revision != save->commits)
        return FODDER_WRITE_FAILED;
    ++save->commits;
    return fodder_profile_encode(after, save->saved, sizeof(save->saved)) ?
           FODDER_WRITE_COMPLETE : FODDER_WRITE_FAILED;
}

static void finish_round(FodderSession *session)
{
    EXPECT(session->battle.count == 1000);
    for (unsigned i = 0; session->screen == FODDER_FIGHT && i < 600; ++i)
        fodder_session_frame(session, 1.0 / 60.0, (FodderInput){0});
    EXPECT(session->screen != FODDER_FIGHT);
}

static float distance(const FodderSoldier *a, const FodderSoldier *b)
{
    return hypotf(a->x - b->x, a->z - b->z);
}

typedef enum { RETREAT, READ_ATTACKS, HOLD_GUARD, SWING_ON_COOLDOWN, STYLES } Style;

static FodderInput fight_with_the_line(const FodderBattle *battle, Style style)
{
    const FodderSoldier *player = battle->soldiers;
    int nearest = -1;
    float closest = 10000.0f;
    for (unsigned i = battle->starting_per_side; i < battle->count; ++i) {
        const FodderSoldier *enemy = &battle->soldiers[i];
        if (enemy->health <= 0.0f)
            continue;
        float range = distance(player, enemy);
        if (range < closest) {
            closest = range;
            nearest = (int)i;
        }
    }

    int incoming = battle->attacker;
    bool threatened = incoming >= 0 && distance(player, &battle->soldiers[incoming]) < 2.2f;
    bool counter = style == READ_ATTACKS && battle->counter_time > 0.25f && battle->counter_target >= 0;
    int target = counter ? battle->counter_target : style == READ_ATTACKS && threatened ? incoming : nearest;
    if (target < 0)
        return (FodderInput){0};
    const FodderSoldier *enemy = &battle->soldiers[target];
    float range = distance(player, enemy);
    FodderInput input = {.look_x = fodder_bearing(player, enemy)};
    input.block = style == HOLD_GUARD ? range < 3.0f :
                  style == READ_ATTACKS && !counter && threatened && enemy->windup <= 0.19f;
    bool opening = counter || (range < 2.15f && (style == SWING_ON_COOLDOWN || !threatened));
    input.attack = style != HOLD_GUARD && opening && !battle->attack_held &&
                   player->windup == 0.0f && player->recovery == 0.0f;

    // Keep pace with allies, then hold ground once involved in the fight.
    if (range > 2.25f && player->health > 20.0f && !input.block && battle->deeds < 4)
        input.move = 0.30f;
    else if (range < 1.60f && (player->health < 50.0f || battle->stamina < 0.24f))
        input.move = -1.0f;
    if (range > 2.5f && range < 12.0f && fabsf(player->vx) + fabsf(player->vz) < 0.15f)
        input.strafe = (battle->tick / 100) % 2 ? -0.35f : 0.35f;
    return input;
}

static void verify_roster(const FodderBattle *battle, float ally_origin)
{
    EXPECT(battle->tick > 100 && battle->count == FODDER_SOLDIERS);
    EXPECT(battle->alive[0] < FODDER_ARMY_SIZE && battle->alive[1] < FODDER_ARMY_SIZE);
    EXPECT(battle->soldiers[49].z != ally_origin);
    unsigned counts[2] = {0}, flankers = 0;
    for (unsigned i = 0; i < battle->count; ++i) {
        const FodderSoldier *soldier = &battle->soldiers[i];
        EXPECT(soldier->side == (i < FODDER_ARMY_SIZE ? 0 : 1));
        if (soldier->health > 0.0f)
            ++counts[soldier->side];
        if (soldier->flank_stage)
            ++flankers;
    }
    EXPECT(counts[0] == battle->alive[0] && counts[1] == battle->alive[1]);
    EXPECT(flankers == 2 * battle->variant.flankers);
}

static void play_full_battles(const FodderPack *pack)
{
    static const char *const styles[] = {"retreat", "read attacks", "hold guard", "swing on cooldown"};
    FodderBattle battle;
    FodderEffects base = fodder_pack_effects(pack, (unsigned[]){0, 0, 0});
    clock_t started = clock();
    unsigned retreat_deaths = 0, decorated_survivors = 0;
    unsigned deeds[STYLES] = {0};
    float lost_health[STYLES] = {0};
    for (unsigned index = 0; index < pack->variant_count; ++index) {
        const FodderVariant *variant = &pack->variants[index];
        unsigned limit = FODDER_TICKS_PER_SECOND * variant->seconds;
        for (Style style = RETREAT; style < STYLES; ++style) {
            fodder_battle_start(&battle, &pack->arena, variant, base, 933 + index, FODDER_ARMY_SIZE);
            float ally_origin = battle.soldiers[49].z;
            bool early_warning = false;
            for (unsigned tick = 0; tick < limit && battle.outcome == FODDER_ACTIVE; ++tick) {
                FodderInput input = style == RETREAT ? (FodderInput){.move = tick < 370 ? -1.0f : 0.0f} :
                                    fight_with_the_line(&battle, style);
                fodder_battle_tick(&battle, input);
                if (battle.flanker >= 0 && distance(battle.soldiers, &battle.soldiers[battle.flanker]) > 11.0f)
                    early_warning = true;
            }
            verify_roster(&battle, ally_origin);
            EXPECT(battle.outcome != FODDER_ACTIVE);
            if (style == RETREAT) {
                EXPECT(early_warning && battle.deeds == 0);
                retreat_deaths += battle.outcome == FODDER_FALLEN;
            }
            if (style == READ_ATTACKS && battle.outcome == FODDER_SURVIVED && battle.deeds >= FODDER_DEEDS_FOR_BONUS)
                ++decorated_survivors;
            if (battle.outcome == FODDER_SURVIVED)
                EXPECT(battle.end == FODDER_ROUT || battle.end == FODDER_HORN);
            float lost = 100.0f - battle.soldiers[0].health;
            lost_health[style] += lost;
            deeds[style] += battle.deeds;
            printf("%s, %s: %s at %.1fs, %.1f health lost, %u deeds.\n", variant->name, styles[style],
                   battle.outcome == FODDER_SURVIVED ? "survived" : "fell",
                   (double)battle.tick / FODDER_TICKS_PER_SECOND, lost, battle.deeds);
        }
    }
    EXPECT(2 * retreat_deaths > pack->variant_count && 2 * decorated_survivors > pack->variant_count);
    EXPECT(lost_health[READ_ATTACKS] < fminf(lost_health[HOLD_GUARD], lost_health[SWING_ON_COOLDOWN]));
    EXPECT(deeds[READ_ATTACKS] > deeds[HOLD_GUARD] && deeds[READ_ATTACKS] > deeds[SWING_ON_COOLDOWN]);
    printf("%u full 1,000-soldier simulations ran in %.3f CPU seconds.\n",
           STYLES * pack->variant_count, (double)(clock() - started) / CLOCKS_PER_SEC);
}

static void fight_frames(FodderSession *session, unsigned ticks, FodderInput input)
{
    for (unsigned i = 0; i < ticks; ++i)
        fodder_session_frame(session, 1.0 / FODDER_TICKS_PER_SECOND, input);
}

int main(int argc, char **argv)
{
    EXPECT(argc == 2);
    FodderPack pack;
    char error[FODDER_ERROR];
    EXPECT(fodder_pack_open(&pack, argv[1], error));

    play_full_battles(&pack);

    Save save = {0};
    FodderProfile reload = {0};
    FodderSession session;
    pack.variants[0].seconds = 2;
    pack.variants[0].enemy_damage = 0;
    fodder_session_open(&session, &pack, reload, record, &save);
    fodder_session_action(&session, FODDER_CONFIRM);
    finish_round(&session);
    EXPECT(session.screen == FODDER_REWARD && session.profile.last_kills == 0 && save.commits == 1);
    EXPECT(fodder_profile_decode(&reload, save.saved, pack.variant_count) && fodder_profile_reward(&reload) == 1);
    fodder_session_open(&session, &pack, reload, record, &save);
    fodder_session_action(&session, FODDER_THIRD_UPGRADE);
    EXPECT(session.screen == FODDER_CAMP && session.profile.rank[2] == 1 && save.commits == 2);

    pack.variants[1].seconds = 2;
    pack.variants[1].ally_speed = pack.variants[1].enemy_speed = 0.0f;
    fodder_session_action(&session, FODDER_CONFIRM);
    EXPECT(session.battle.effects.dressing > 0 && session.profile.variant == 1);
    FodderSoldier *player = session.battle.soldiers;
    FodderSoldier *enemy = &session.battle.soldiers[FODDER_ARMY_SIZE];
    player->z = 40.0f;
    for (unsigned i = 0; i < 2; ++i) {
        enemy[i].x = player->x;
        enemy[i].z = player->z - 1.3f + 3.0f * (float)i;
        enemy[i].health = 1.0f;
        enemy[i].cooldown = 10.0f;
    }
    fight_frames(&session, 16, (FodderInput){.attack = true});
    EXPECT(session.battle.kills == 1);
    fight_frames(&session, 24, (FodderInput){0});
    fight_frames(&session, 1, (FodderInput){.attack = true, .look_x = 3.1415927f});
    finish_round(&session);
    EXPECT(session.screen == FODDER_REWARD && session.profile.last_kills == 2 && save.commits == 3);
    EXPECT(fodder_profile_decode(&reload, save.saved, pack.variant_count) && fodder_profile_reward(&reload) == 2);
    fodder_session_open(&session, &pack, reload, record, &save);
    fodder_session_action(&session, FODDER_FIRST_UPGRADE);
    EXPECT(session.screen == FODDER_CAMP && session.profile.rank[0] == 2 && save.commits == 4);

    fodder_session_action(&session, FODDER_CONFIRM);
    EXPECT(session.profile.variant == 2);
    player->z = 40.0f;
    enemy->x = player->x;
    enemy->z = player->z - 1;
    enemy->windup = 0.008f;
    enemy->windup_duration = 0.4f;
    enemy->target = 0;
    player->health = 1.0f;
    fodder_session_frame(&session, 1.0 / 30.0, (FodderInput){0});
    EXPECT(session.screen == FODDER_RESULT && !session.profile.pending && save.commits == 5);
    EXPECT(fodder_profile_decode(&reload, save.saved, pack.variant_count) && reload.rank[0] == 2 && reload.rank[2] == 1);
    fodder_session_open(&session, &pack, reload, record, &save);
    EXPECT(session.screen == FODDER_CAMP && session.battle.effects.dressing > 0);
    fodder_pack_close(&pack);
    puts("Survival, repeatable saves, death, and permanent upgrades passed.");
    return 0;
}
