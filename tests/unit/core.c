#include "geometry.h"
#include "session.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define CREATE_DIR(path) _mkdir(path)
#define REMOVE_DIR(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define CREATE_DIR(path) mkdir(path, 0777)
#define REMOVE_DIR(path) rmdir(path)
#endif

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); exit(1); } } while (0)

static const char *const test_root = "fodder-unit-assets";
static const char *const fixture = "fodder-unit-assets/scratch";
static char error[FODDER_ERROR];

static bool close_float(float a, float b)
{
    return fabsf(a - b) < 0.001f;
}

static void content(const char *path, const void *data, size_t length)
{
    FILE *file = fopen(path, "wb");
    CHECK(file);
    CHECK(fwrite(data, 1, length, file) == length);
    CHECK(fclose(file) == 0);
}

static void text(const char *path, const char *value)
{
    content(path, value, strlen(value));
}

static void test_path(void)
{
    char path[20];
    CHECK(fodder_valid_id("levy_1-episode"));
    CHECK(!fodder_valid_id(""));
    CHECK(!fodder_valid_id("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    CHECK(!fodder_valid_id("abcdefghijklmnopqrstuvwxyzaaaaaaa"));
    CHECK(fodder_valid_path("data/files/a.json"));
    CHECK(!fodder_valid_path(""));
    CHECK(!fodder_valid_path("/a"));
    CHECK(!fodder_valid_path("a/"));
    CHECK(!fodder_valid_path("a//b"));
    CHECK(!fodder_valid_path("a/../b"));
    CHECK(!fodder_valid_path("a\\b"));
    CHECK(fodder_path_join(path, sizeof(path), "data", "items"));
    CHECK(!strcmp(path, "data/items"));
    CHECK(!fodder_path_join(path, 4, "data", "items"));
    size_t length;
    CHECK(!fodder_read(fixture, 50, &length));
    text(fixture, "test");
    CHECK(!fodder_read(fixture, 3, &length));
    char *contents = fodder_read(fixture, 4, &length);
    CHECK(contents && length == 4 && !strcmp(contents, "test"));
    free(contents);
}

static void test_profile(void)
{
    FodderProfile p = {0}, q = {0}, r = {0};
    char encoded[FODDER_PROFILE_TEXT];
    CHECK(fodder_profile_encode(&p, encoded, sizeof(encoded)));
    CHECK(!fodder_profile_encode(&p, encoded, 3));
    CHECK(!fodder_profile_decode(&q, "FODDER3 0", 1));
    CHECK(!fodder_profile_decode(&q, "FODDER1 0 -", 1));
    CHECK(!fodder_profile_decode(&q, "FODDER1 0 0", 1));
    CHECK(!fodder_profile_decode(&q, "FODDER1 42949672960 0 0 0 0 0 0 0 0 0 0", 1));
    CHECK(!fodder_profile_decode(&q, "FODDER1 0 0 0 0 0 0 0 0 0 0 1", 1));
    CHECK(!fodder_profile_decode(&q, "FODDER1 0 0 0 0 0 0 0 0 0 0 0", 0));
    uint32_t seed = fodder_profile_seed(&p);
    CHECK(!fodder_profile_result(&q, &p, FODDER_ACTIVE, 0, 0));
    CHECK(!fodder_profile_result(&q, &p, FODDER_FALLEN, FODDER_ARMY_SIZE + 1, 0));
    CHECK(!fodder_profile_claim(&q, &p, 0, 3));
    CHECK(fodder_profile_result(&q, &p, FODDER_SURVIVED, 12, 0));
    CHECK(q.pending && q.streak == 1 && q.best_streak == 1 && q.best_kills == 12);
    CHECK(seed != fodder_profile_seed(&q));
    CHECK(!fodder_profile_result(&r, &q, FODDER_SURVIVED, 12, 0));
    CHECK(!fodder_profile_claim(&r, &q, 3, 3));
    CHECK(!fodder_profile_claim(&r, &q, 0, 0));
    CHECK(fodder_profile_claim(&r, &q, 1, 3));
    CHECK(!r.pending && r.variant == 1 && r.rank[1] == 1 && r.revision == 2);
    CHECK(fodder_profile_result(&q, &r, FODDER_FALLEN, 0, 0));
    CHECK(q.streak == 0 && q.best_streak == 1 && q.rank[1] == 1 && q.best_kills == 12);
    CHECK(fodder_profile_encode(&q, encoded, sizeof(encoded)));
    CHECK(fodder_profile_decode(&p, encoded, 1) && p.variant == 0 && p.rank[1] == 1);
    CHECK(fodder_profile_decode(&p, encoded, 3) && p.variant == 1 && p.attempts == 2);
    CHECK(fodder_profile_decode(&p, "FODDER1 4 2 1 3 0 1 12 1 1 7 1", 3));
    CHECK(p.rank[0] == 3 && p.pending && !p.bonus && fodder_profile_reward(&p) == 1);
    CHECK(!fodder_profile_decode(&p, "FODDER2 4 2 1 3 0 1 12 1 1 7 0 1", 3));
    CHECK(fodder_profile_result(&q, &r, FODDER_SURVIVED, 2, FODDER_DEEDS_FOR_BONUS));
    CHECK(q.bonus && fodder_profile_encode(&q, encoded, sizeof(encoded)));
    CHECK(fodder_profile_decode(&p, encoded, 3) && fodder_profile_reward(&p) == 2);
    CHECK(fodder_profile_claim(&r, &p, 1, 3) && r.rank[1] == 3 && !r.bonus);
    CHECK(!fodder_profile_claim(&q, &r, 1, 3));
    q.revision = UINT_MAX;
    CHECK(!fodder_profile_result(&p, &q, FODDER_FALLEN, 0, 0));
    q.pending = true;
    CHECK(!fodder_profile_claim(&p, &q, 2, 3));
}

static void wrap_rules(const FodderPack *pack, const char *extra)
{
    char p[FODDER_PATH];
    size_t length;
    CHECK(fodder_pack_file(pack, FODDER_SCRIPT, p));
    char *base = fodder_read(p, 20000, &length);
    CHECK(base);
    char *value = malloc(length + strlen(extra) + 100);
    CHECK(value);
    snprintf(value, length + strlen(extra) + 100, "local rules = (function()\n%s\nend)()\n%s\nreturn rules\n",
             base, extra);
    text("fodder-unit-assets/rules.lua", value);
    free(value);
    free(base);
}

static void test_pack(const FodderPack *pack)
{
    FodderPack fixture_pack;
    CHECK(!fodder_pack_open(&fixture_pack, "missing-test-pack", error));
    CHECK(!fodder_pack_open(&fixture_pack, "fodder-unit-assets/rules.lua", error));
    char bad_root[FODDER_PATH + 2];
    memset(bad_root, 'a', sizeof(bad_root) - 1);
    bad_root[sizeof(bad_root) - 1] = 0;
    CHECK(!fodder_pack_open(&fixture_pack, bad_root, error));
    const char *manifest = "fodder-unit-assets/pack.json";
    const char *script = "fodder-unit-assets/rules.lua";
    text(manifest, "{malformed}");
    CHECK(!fodder_pack_open(&fixture_pack, test_root, error));
    text(manifest, "{\"id\":\"mod-pack\",\"api\":1,\"title\":\"Edited pack\"}");
    CHECK(!fodder_pack_open(&fixture_pack, test_root, error));
    text(manifest, "{\"id\":\"mod-pack\",\"api\":1,\"title\":\"Edited pack\",\"files\":{"
         "\"script\":\"rules.lua\",\"soldier\":\"a.mesh\",\"sword\":\"b.mesh\",\"shield\":\"c.mesh\","
         "\"sprite\":\"a.tga\",\"banner\":\"b.tga\",\"strike\":\"a.wav\",\"horn\":\"b.wav\",\"crowd\":\"c.wav\"}}");
    CHECK(!fodder_pack_open(&fixture_pack, test_root, error));
    const char *invalid[] = {
        "return if", "return false", "while true do end", "return string.rep('x', 5000000)",
        "return {arena = 5}", "return {arena = {name = true, obstacles = false}}"
    };
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        text(script, invalid[i]);
        CHECK(!fodder_pack_open(&fixture_pack, test_root, error));
        fodder_pack_close(&fixture_pack);
    }
    const char *mutations[] = {
        "rules.arena.obstacles[1] = 12", "rules.battles = {}", "rules.battles[1] = true",
        "rules.battles[1].hint = '\\n'", "rules.arena.width = 0/0", "rules.upgrades = true",
        "rules.upgrades[1] = true", "rules.effects = false", "rules.battles[1].flankers = true",
        "rules.effects = function() return false end",
        "rules.effects = function() return { recovery=3, block_cost=1, dressing=0 } end"
    };
    for (unsigned i = 0; i < sizeof(mutations) / sizeof(mutations[0]); ++i) {
        wrap_rules(pack, mutations[i]);
        CHECK(!fodder_pack_open(&fixture_pack, test_root, error));
    }
    wrap_rules(pack, "local effects = rules.effects; rules.battles[1].flankers = nil; "
                     "rules.effects = function(a,b,c) if a == 3 then error({}) end; return effects(a,b,c) end");
    CHECK(fodder_pack_open(&fixture_pack, test_root, error));
    CHECK(!error[0] && fixture_pack.variant_count == 3 && fixture_pack.variants[0].flankers == 0);
    char path[FODDER_PATH];
    CHECK(!fodder_pack_file(&fixture_pack, FODDER_FLANK_AUDIO, path));
    CHECK(fodder_pack_file(pack, FODDER_FLANK_AUDIO, path));
    unsigned ranks[] = {3, 8, 5};
    FodderEffects fallback = fodder_pack_effects(&fixture_pack, ranks);
    CHECK(fallback.recovery == 1 && fallback.block_cost == 1 && fallback.dressing == 0);
    ranks[0] = 6;
    CHECK(fodder_pack_effects(&fixture_pack, ranks).recovery > 1);
    fodder_pack_close(&fixture_pack);
    remove(script);
    remove(manifest);
}

static void test_geometry(const FodderPack *pack)
{
    FodderMatrix identity = fodder_identity();
    FodderMatrix moved = fodder_transform((FodderVec){1, 2, 3}, 0.2f, -0.1f, 0, 1);
    FodderMatrix view = fodder_view((FodderVec){1, 2, 3}, 0.2f, -0.1f);
    FodderMatrix together = fodder_multiply(view, moved);
    for (unsigned i = 0; i < 16; ++i)
        CHECK(close_float(identity.data[i], together.data[i]));
    FodderMatrix perspective = fodder_perspective(1.2f, 2, 0.05f, 100);
    CHECK(perspective.data[5] > perspective.data[0] && perspective.data[11] == -1);

    FodderMesh mesh = {0};
    CHECK(!fodder_mesh_read(&mesh, "missing.mesh"));
    text(fixture, "FODDERM1 2");
    CHECK(!fodder_mesh_read(&mesh, fixture));
    text(fixture, "FODDERM1 3\n0 0 0 1 0 0 1\n99 0 0 0 0 0 0\n0 0 0 8 0 0 1\n");
    CHECK(!fodder_mesh_read(&mesh, fixture) && !mesh.vertices);
    char path[FODDER_PATH];
    CHECK(fodder_pack_file(pack, FODDER_SOLDIER_MESH, path) && fodder_mesh_read(&mesh, path));
    CHECK(mesh.count > 100);
    unsigned arm_vertices = 0;
    for (unsigned i = 0; i < mesh.count; ++i)
        arm_vertices += mesh.vertices[i].part == 1;
    CHECK(arm_vertices > 30);
    fodder_mesh_free(&mesh);
    text(fixture, "FODDERM2 3\n0 0 0 1 0 0 1 3\n");
    CHECK(!fodder_mesh_read(&mesh, fixture));
    CHECK(fodder_landscape(&mesh, &pack->arena) && mesh.count > 10000);
    fodder_mesh_free(&mesh);
    CHECK(fodder_banners(&mesh, &pack->arena) && mesh.count == 24);
    fodder_mesh_free(&mesh);

    FodderImage image = {0};
    CHECK(!fodder_image_read(&image, "missing.tga"));
    CHECK(!fodder_image_read(&image, fixture));
    const unsigned char zero[] = {0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,32,8};
    content(fixture, zero, sizeof(zero));
    CHECK(!fodder_image_read(&image, fixture));
    CHECK(fodder_pack_file(pack, FODDER_SPRITE, path) && fodder_image_read(&image, path));
    CHECK(image.width == 28 && image.height == 40);
    fodder_image_free(&image);
    CHECK(!image.pixels);

    unsigned char font[127][7] = {0};
    CHECK(!fodder_font_read(font, "missing-font.txt"));
    text(fixture, "28 0 0 0 0 0 0 0");
    CHECK(!fodder_font_read(font, fixture));
    text(fixture, "48 80");
    CHECK(!fodder_font_read(font, fixture));
    text(fixture, "48 1 1 1 1 1 1 1\n65 1 1 1 1 1 1 1\n");
    CHECK(fodder_font_read(font, fixture) && font['0'][0] == 1);

    CHECK(close_float(fodder_ground(&pack->arena, 0, 25), 0));
    CHECK(fodder_ground(&pack->arena, 25, 0) < fodder_ground(&pack->arena, 0, 0));
}

static void settle_near(FodderBattle *battle, bool in_front)
{
    FodderSoldier *attacker = &battle->soldiers[2];
    attacker->x = battle->soldiers[0].x;
    attacker->z = battle->soldiers[0].z + (in_front ? -1.5f : 1.5f);
    attacker->yaw = in_front ? 3.1415927f : 0;
    attacker->windup = 0.005f;
    attacker->windup_duration = 0.4f;
    attacker->strike = FODDER_STRIKE_SLASH;
    attacker->target = 0;
}

static void test_battle(const FodderPack *pack)
{
    FodderBattle battle;
    FodderArena arena = pack->arena;
    FodderVariant variant = pack->variants[0];
    FodderEffects effects = {1, 1, 10};
    fodder_battle_start(&battle, &arena, &variant, effects, 0, FODDER_ARMY_SIZE);
    CHECK(battle.count == 1000 && battle.alive[0] == 500 && battle.alive[1] == 500);
    CHECK(battle.soldiers[499].side == 0 && battle.soldiers[500].side == 1);
    for (unsigned i = 0; i < 3050; ++i)
        fodder_battle_tick(&battle, (FodderInput){0});
    CHECK(battle.alive[0] < 500 && battle.alive[1] < 500);

    variant.rout_fraction = 0.5f;
    variant.enemy_damage = 15.0f / 2.65f;
    fodder_battle_start(&battle, &arena, &variant, effects, 42, 2);
    battle.block_held = true;
    settle_near(&battle, true);
    fodder_battle_tick(&battle, (FodderInput){.block = true});
    CHECK(battle.stamina < 0.9f && battle.soldiers[0].health > 96);
    battle.soldiers[0].health = 40;
    settle_near(&battle, false);
    fodder_battle_tick(&battle, (FodderInput){.block = true});
    CHECK(battle.dressed && close_float(battle.soldiers[0].health, 35));
    settle_near(&battle, true);
    battle.soldiers[2].windup = 0;
    battle.soldiers[2].health = 1;
    for (unsigned i = 0; i < 16; ++i)
        fodder_battle_tick(&battle, (FodderInput){.attack = true});
    CHECK(battle.kills == 1 && battle.outcome == FODDER_SURVIVED &&
          battle.end == FODDER_ROUT && battle.winner == 0);
    unsigned fin = battle.tick;
    fodder_battle_tick(&battle, (FodderInput){0});
    CHECK(battle.tick == fin);

    fodder_battle_start(&battle, &arena, &variant, effects, 8, 2);
    battle.soldiers[1].health = 0;
    battle.alive[0] = 1;
    fodder_battle_tick(&battle, (FodderInput){0});
    CHECK(battle.outcome == FODDER_SURVIVED && battle.end == FODDER_ROUT && battle.winner == 1);

    variant.seconds = 1;
    variant.rout_fraction = 0;
    fodder_battle_start(&battle, &arena, &variant, effects, 1, 2);
    battle.tick = 59;
    settle_near(&battle, true);
    battle.soldiers[0].health = 1;
    fodder_battle_tick(&battle, (FodderInput){0});
    CHECK(battle.outcome == FODDER_FALLEN && battle.end == FODDER_HORN);
    fodder_battle_start(&battle, &arena, &variant, effects, 1, 2);
    settle_near(&battle, false);
    battle.soldiers[0].health = 1;
    fodder_battle_tick(&battle, (FodderInput){0});
    CHECK(battle.end == FODDER_DEATH && battle.outcome == FODDER_FALLEN);
    fodder_battle_start(&battle, &arena, &variant, effects, 1, 2);
    arena.obstacles[0] = (FodderObstacle){0, 14.6f, 1, 0.5f, 2};
    battle.soldiers[0].z = 15.5f;
    for (unsigned i = 0; i < 20; ++i)
        fodder_battle_tick(&battle, (FodderInput){.move = 2});
    CHECK(battle.soldiers[0].z > 15.3f);
    CHECK(close_float(fodder_wrap(8), 1.7168f));
}

static void ticks(FodderBattle *battle, unsigned count, FodderInput input)
{
    for (unsigned i = 0; i < count; ++i)
        fodder_battle_tick(battle, input);
}

static void test_melee(const FodderPack *pack)
{
    FodderArena arena = pack->arena;
    arena.obstacle_count = 0;
    FodderVariant variant = pack->variants[0];
    variant.ally_speed = variant.enemy_speed = 0;
    variant.rout_fraction = 0;
    FodderEffects effects = {1, 1, 0};
    FodderBattle battle;
    fodder_battle_start(&battle, &arena, &variant, effects, 9, 2);
    FodderSoldier *player = &battle.soldiers[0], *ally = &battle.soldiers[1];
    FodderSoldier *enemy = &battle.soldiers[2];
    ally->x = player->x;
    ally->z = player->z - 2.0f;
    ticks(&battle, 40, (FodderInput){.move = 1});
    CHECK(player->z - ally->z >= 0.699f && player->z - ally->z < 0.80f);
    ticks(&battle, 15, (FodderInput){.strafe = 1});
    ticks(&battle, 35, (FodderInput){.move = 1});
    CHECK(player->z < ally->z && player->x > 0.70f);
    enemy->x = player->x;
    enemy->z = player->z - 1.7f;
    enemy->cooldown = 30;
    ticks(&battle, 35, (FodderInput){.move = 1});
    CHECK(player->z - enemy->z >= 0.699f && player->z - enemy->z < 0.80f);
    enemy->health = 0;
    --battle.alive[1];
    ticks(&battle, 30, (FodderInput){.move = 1});
    CHECK(player->z < enemy->z);

    // A moving ally changes grid cells just before a second ally tries to enter.
    fodder_battle_start(&battle, &arena, &variant, effects, 9, 3);
    player->x = player->z = 48;
    *ally = (FodderSoldier){.x = 3.95f, .z = 18, .vx = 4, .health = 100, .target = -1};
    *enemy = (FodderSoldier){.x = 4.73f, .z = 18, .vx = -4, .health = 100, .target = -1};
    ticks(&battle, 2, (FodderInput){0});
    CHECK(ally->x > 4.0f && enemy->x - ally->x >= 0.699f);

    fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
    ally->x = player->x;
    ally->z = player->z - 0.95f;
    enemy->x = player->x;
    enemy->z = player->z - 1.9f;
    enemy->cooldown = 30;
    CHECK(fodder_strike_phase(player) == 0);
    ticks(&battle, 10, (FodderInput){.attack = true});
    CHECK(fodder_strike_phase(player) > 0 && fodder_strike_phase(player) < 0.42f);
    ticks(&battle, 6, (FodderInput){.attack = true});
    CHECK(fodder_strike_phase(player) >= 0.42f && battle.feedback == FODDER_COMBAT_OBSTRUCTED && battle.sword_recoil);
    CHECK(enemy->health == 100.0f);
    ally->x = 3;
    ticks(&battle, 85, (FodderInput){.attack = true});
    CHECK(fodder_strike_phase(player) == 0 && enemy->health == 100.0f);
    ticks(&battle, 1, (FodderInput){0});
    ticks(&battle, 16, (FodderInput){.attack = true});
    CHECK(battle.feedback == FODDER_COMBAT_HIT && enemy->health < 100.0f);

    fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
    settle_near(&battle, true);
    CHECK(fodder_strike_phase(enemy) > 0.40f);
    ticks(&battle, 1, (FodderInput){.block = true});
    CHECK(player->health == 100.0f && enemy->stagger > 0 && battle.counter_target == 2 &&
          battle.counter_time > 0 && battle.deeds == 0);
    ticks(&battle, 16, (FodderInput){.attack = true});
    CHECK(player->health == 100.0f && enemy->health == 0 && battle.feedback == FODDER_COMBAT_COUNTER);
    CHECK(battle.kills == 1 && battle.deeds == 1 && battle.counter_time == 0);

    fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
    settle_near(&battle, true);
    enemy->x = player->x - 1.5f;
    enemy->z = player->z;
    ticks(&battle, 1, (FodderInput){0});
    CHECK(enemy->recovery > 0.0f && player->health == 100.0f);

    fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
    enemy->x = player->x;
    enemy->z = player->z - 1.5f;
    enemy->cooldown = 30;
    ticks(&battle, 16, (FodderInput){.attack = true});
    CHECK(battle.feedback == FODDER_COMBAT_BLOCKED && enemy->health > 100.0f - variant.player_damage);

    fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
    arena.obstacle_count = 1;
    arena.obstacles[0] = (FodderObstacle){0, player->z - 0.6f, 0.3f, 0.11f, 2};
    settle_near(&battle, true);
    ticks(&battle, 16, (FodderInput){.attack = true});
    CHECK(battle.feedback == FODDER_COMBAT_OBSTRUCTED && enemy->health == 100.0f && player->health == 100.0f);
}

static void test_combat_choices(const FodderPack *pack)
{
    FodderArena arena = pack->arena;
    arena.obstacle_count = 0;
    FodderVariant variant = pack->variants[0];
    variant.ally_speed = variant.enemy_speed = 0.0f;
    FodderEffects effects = {1, 1, 0};
    FodderBattle battle;
    FodderSoldier *player = battle.soldiers;
    FodderSoldier *foe = &battle.soldiers[2], *other = &battle.soldiers[3];

    for (unsigned pattern = FODDER_STRIKE_SLASH; pattern <= FODDER_STRIKE_OVERHEAD; ++pattern) {
        fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
        settle_near(&battle, true);
        foe->strike = (uint8_t)pattern;
        battle.block_held = true;
        ticks(&battle, 1, (FodderInput){.block = true});
        float cost = pattern == FODDER_STRIKE_SLASH ? 0.17f : 0.34f;
        CHECK(close_float(battle.stamina, 1.0f - cost) && battle.feedback == FODDER_COMBAT_GUARDED);
        CHECK(battle.feedback_tick == battle.tick && player->health > 96.0f);

        fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
        settle_near(&battle, true);
        foe->strike = (uint8_t)pattern;
        ticks(&battle, 1, (FodderInput){.block = true});
        CHECK(close_float(battle.stamina, 0.94f) && battle.feedback == FODDER_COMBAT_PARRY);
        CHECK(player->health == 100.0f && fodder_counter_opponent(&battle) == 2);
    }

    fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
    settle_near(&battle, true);
    foe->strike = FODDER_STRIKE_OVERHEAD;
    battle.stamina = 0.2f;
    battle.block_held = true;
    ticks(&battle, 1, (FodderInput){.block = true});
    CHECK(battle.feedback == FODDER_COMBAT_GUARD_BROKEN && battle.stamina == 0 && !player->guarding);
    CHECK(player->health < 85.0f);

    fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
    settle_near(&battle, true);
    other->x = 0.8f;
    other->z = player->z - 1.0f;
    other->cooldown = 30.0f;
    ticks(&battle, 1, (FodderInput){.block = true});
    CHECK(fodder_counter_opponent(&battle) == 2 && fodder_bearing(player, other) > 0.60f);
    battle.counter_time = 0.08f;
    ticks(&battle, 1, (FodderInput){.attack = true, .look_x = 0.60f});
    ticks(&battle, 7, (FodderInput){0});
    CHECK(battle.counter_time == 0 && fodder_counter_opponent(&battle) == 2);
    ticks(&battle, 8, (FodderInput){0});
    CHECK(foe->health == 0 && other->health == 100.0f && battle.feedback == FODDER_COMBAT_COUNTER);

    fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
    settle_near(&battle, true);
    ticks(&battle, 1, (FodderInput){.block = true});
    battle.soldiers[1].x = 0.0f;
    battle.soldiers[1].z = player->z - 0.74f;
    ticks(&battle, 16, (FodderInput){.attack = true});
    CHECK(foe->health == 100.0f && battle.feedback == FODDER_COMBAT_OBSTRUCTED && battle.sword_recoil);

    fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
    settle_near(&battle, true);
    other->x = 1.2f;
    other->z = player->z - 0.6f;
    other->cooldown = 30.0f;
    ticks(&battle, 1, (FodderInput){.block = true});
    ticks(&battle, 1, (FodderInput){.attack = true});
    ticks(&battle, 1, (FodderInput){.look_x = 1.1f});
    ticks(&battle, 14, (FodderInput){0});
    CHECK(foe->health == 100.0f && other->health == 100.0f && battle.feedback == FODDER_COMBAT_MISS);

    fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
    foe->x = 0;
    foe->z = player->z - 2.1f;
    other->x = 0.8f;
    other->z = player->z - 1.3f;
    other->cooldown = 30.0f;
    ticks(&battle, 16, (FodderInput){.attack = true});
    CHECK(foe->health < 100.0f && other->health == 100.0f && battle.feedback == FODDER_COMBAT_HIT);
    CHECK(foe->provoked_until > battle.tick);

    fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
    ticks(&battle, 16, (FodderInput){.attack = true});
    CHECK(battle.feedback == FODDER_COMBAT_MISS && !battle.sword_recoil);

    for (unsigned pattern = FODDER_STRIKE_SLASH; pattern <= FODDER_STRIKE_OVERHEAD; ++pattern) {
        fodder_battle_start(&battle, &arena, &variant, effects, 19, 1);
        FodderSoldier *attacker = &battle.soldiers[1];
        attacker->z = player->z - 1.6f;
        attacker->x = 0.0f;
        attacker->cooldown = 0.0f;
        attacker->strike = pattern == FODDER_STRIKE_SLASH ? FODDER_STRIKE_OVERHEAD : FODDER_STRIKE_SLASH;
        ticks(&battle, 2, (FodderInput){0});
        CHECK(battle.attacker == 1 && attacker->strike == pattern);
        float duration = pattern == FODDER_STRIKE_SLASH ? 0.45f : 0.65f;
        CHECK(close_float(attacker->windup, duration));
        unsigned strike_ticks = pattern == FODDER_STRIKE_SLASH ? 27 : 39;
        ticks(&battle, strike_ticks - 1, (FodderInput){0});
        CHECK(battle.attacker == 1 && player->health == 100.0f);
        ticks(&battle, 1, (FodderInput){0});
        CHECK(battle.attacker == -1 && player->health < 100.0f && attacker->recovery > 0.0f);

        fodder_battle_start(&battle, &arena, &variant, effects, 15, 2);
        settle_near(&battle, true);
        foe->x = 1.02f;
        foe->z = player->z - 1.30f;
        foe->strike = (uint8_t)pattern;
        ticks(&battle, 1, (FodderInput){0});
        CHECK((player->health == 100.0f) == (pattern == FODDER_STRIKE_OVERHEAD));
    }
}

typedef struct {
    FodderWrite writes;
    unsigned count;
} Storage;

static FodderWrite save(void *data, const FodderProfile *previous, const FodderProfile *next)
{
    Storage *store = data;
    CHECK(next->revision == previous->revision + 1);
    ++store->count;
    return store->writes;
}

static void test_session(FodderPack *pack)
{
    Storage store = {.writes = FODDER_WRITE_PENDING};
    FodderSession app;
    FodderProfile profile = {0};
    pack->variants[0].seconds = 1;
    fodder_session_open(&app, pack, profile, save, &store);
    CHECK(app.screen == FODDER_CAMP);
    fodder_session_frame(&app, 0.1, (FodderInput){0});
    fodder_session_action(&app, FODDER_BACK);
    fodder_session_action(&app, FODDER_CONFIRM);
    CHECK(app.screen == FODDER_FIGHT);
    fodder_session_action(&app, FODDER_BACK);
    CHECK(app.screen == FODDER_PAUSED);
    fodder_session_action(&app, FODDER_CONFIRM);
    fodder_session_frame(&app, 0.005, (FodderInput){.look_x = 0.3f});
    CHECK(app.queued_x > 0 && app.battle.tick == 0);
    fodder_session_frame(&app, 0.015, (FodderInput){.look_x = 0.1f});
    CHECK(close_float(app.battle.soldiers[0].yaw, 0.4f));
    fodder_session_frame(&app, 0.004, (FodderInput){.attack = true});
    CHECK(app.queued_attack && fodder_strike_phase(app.battle.soldiers) == 0);
    fodder_session_frame(&app, 0.012, (FodderInput){0});
    CHECK(!app.queued_attack && fodder_strike_phase(app.battle.soldiers) > 0);
    app.battle.deeds = FODDER_DEEDS_FOR_BONUS;
    app.battle.tick = 59;
    fodder_session_frame(&app, 0.03, (FodderInput){0});
    CHECK(app.saving && store.count == 1);
    fodder_session_action(&app, FODDER_CONFIRM);
    CHECK(store.count == 1);
    fodder_session_saved(&app, false);
    CHECK(app.save_error);
    fodder_session_action(&app, FODDER_BACK);
    CHECK(store.count == 1);
    store.writes = FODDER_WRITE_COMPLETE;
    fodder_session_action(&app, FODDER_CONFIRM);
    CHECK(app.screen == FODDER_REWARD && store.count == 2);
    fodder_session_action(&app, FODDER_PREVIOUS);
    CHECK(app.selection == 2);
    fodder_session_action(&app, FODDER_NEXT);
    CHECK(app.selection == 0);
    fodder_session_action(&app, FODDER_SECOND_UPGRADE);
    CHECK(app.screen == FODDER_CAMP && app.profile.rank[1] == 2);
    CHECK(app.profile.variant == 1 && app.battle.effects.block_cost < 1.0f);
    fodder_session_saved(&app, true);
    CHECK(store.count == 3);
    profile.pending = true;
    profile.streak = 1;
    fodder_session_open(&app, pack, profile, save, &store);
    CHECK(app.screen == FODDER_REWARD);
    fodder_session_action(&app, FODDER_CONFIRM);
    CHECK(app.profile.rank[0] == 1);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    (void)CREATE_DIR(test_root);
    remove(fixture);
    FodderPack pack;
    CHECK(fodder_pack_open(&pack, argv[1], error));
    test_path();
    test_profile();
    test_pack(&pack);
    test_geometry(&pack);
    test_battle(&pack);
    test_melee(&pack);
    test_combat_choices(&pack);
    test_session(&pack);
    fodder_pack_close(&pack);
    remove(fixture);
    REMOVE_DIR(test_root);
    puts("Fodder core unit tests passed.");
    return 0;
}
