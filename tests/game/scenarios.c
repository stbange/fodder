#include "session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static void complete_round(FodderSession *session)
{
    fodder_session_action(session, FODDER_CONFIRM);
    EXPECT(session->battle.count == 1000);
    for (unsigned i = 0; session->screen == FODDER_FIGHT && i < 600; ++i)
        fodder_session_frame(session, 1.0 / 60.0, (FodderInput){0});
    EXPECT(session->screen != FODDER_FIGHT);
}

int main(int argc, char **argv)
{
    EXPECT(argc == 2);
    FodderPack pack;
    char error[FODDER_ERROR];
    EXPECT(fodder_pack_open(&pack, argv[1], error));

    FodderBattle battle;
    FodderEffects base = fodder_pack_effects(&pack, (unsigned[]){0, 0, 0});
    clock_t started = clock();
    for (unsigned index = 0; index < pack.variant_count; ++index) {
        const FodderVariant *variant = &pack.variants[index];
        fodder_battle_start(&battle, &pack.arena, variant, base, 933 + index, FODDER_ARMY_SIZE);
        float ally_origin = battle.soldiers[49].z;
        unsigned limit = FODDER_TICKS_PER_SECOND * variant->seconds;
        for (unsigned tick = 0; tick < limit && battle.outcome == FODDER_ACTIVE; ++tick) {
            FodderInput input = {.move = tick < 370 ? -1.0f : 0, .look_x = tick == 370 ? 3.14f : 0};
            fodder_battle_tick(&battle, input);
        }
        EXPECT(battle.tick > 100 && battle.outcome == FODDER_SURVIVED);
        EXPECT(battle.end == FODDER_ROUT || battle.end == FODDER_HORN);
        printf("%s reached its battle end at %.1f simulated seconds.\n",
               variant->name, (double)battle.tick / FODDER_TICKS_PER_SECOND);
        EXPECT(battle.alive[0] < 500 && battle.alive[1] < 500);
        EXPECT(battle.soldiers[49].z != ally_origin);
        unsigned counts[2] = {0};
        for (unsigned i = 0; i < battle.count; ++i) {
            EXPECT(battle.soldiers[i].side == (i < 500 ? 0 : 1));
            if (battle.soldiers[i].health > 0) ++counts[battle.soldiers[i].side];
        }
        EXPECT(counts[0] == battle.alive[0] && counts[1] == battle.alive[1]);
    }
    printf("%u complete 1,000-soldier simulations ran in %.3f CPU seconds.\n",
           pack.variant_count, (double)(clock() - started) / CLOCKS_PER_SEC);

    Save save = {0};
    FodderProfile reload = {0};
    FodderSession session;
    pack.variants[0].seconds = 2;
    pack.variants[0].enemy_damage = 0;
    fodder_session_open(&session, &pack, reload, record, &save);
    complete_round(&session);
    EXPECT(session.screen == FODDER_REWARD && session.profile.last_kills == 0 && save.commits == 1);
    EXPECT(fodder_profile_decode(&reload, save.saved, pack.variant_count) && reload.pending);
    fodder_session_open(&session, &pack, reload, record, &save);
    fodder_session_action(&session, FODDER_THIRD_UPGRADE);
    EXPECT(session.screen == FODDER_CAMP && session.profile.rank[2] == 1 && save.commits == 2);

    fodder_session_action(&session, FODDER_CONFIRM);
    EXPECT(session.battle.effects.dressing > 0 && session.profile.variant == 1);
    FodderSoldier *enemy = &session.battle.soldiers[500];
    enemy->x = 0;
    enemy->z = session.battle.soldiers[0].z - 1;
    enemy->windup = 0.008f;
    enemy->target = 0;
    session.battle.soldiers[0].health = 1;
    fodder_session_frame(&session, 1.0 / 30.0, (FodderInput){0});
    EXPECT(session.screen == FODDER_RESULT && !session.profile.pending);
    EXPECT(fodder_profile_decode(&reload, save.saved, pack.variant_count) && reload.rank[2] == 1);
    fodder_session_open(&session, &pack, reload, record, &save);
    EXPECT(session.screen == FODDER_CAMP && session.battle.effects.dressing > 0);
    fodder_pack_close(&pack);
    puts("Survival, repeatable saves, death, and permanent upgrades passed.");
    return 0;
}
