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
    CHECK(!fodder_profile_decode(&q, "FODDER2 0", 1));
    CHECK(!fodder_profile_decode(&q, "FODDER1 0 -", 1));
    CHECK(!fodder_profile_decode(&q, "FODDER1 0 0", 1));
    CHECK(!fodder_profile_decode(&q, "FODDER1 42949672960 0 0 0 0 0 0 0 0 0 0", 1));
    CHECK(!fodder_profile_decode(&q, "FODDER1 0 0 0 0 0 0 0 0 0 0 1", 1));
    CHECK(!fodder_profile_decode(&q, "FODDER1 0 0 0 0 0 0 0 0 0 0 0", 0));
    uint32_t seed = fodder_profile_seed(&p);
    CHECK(!fodder_profile_result(&q, &p, FODDER_ACTIVE, 0));
    CHECK(!fodder_profile_result(&q, &p, FODDER_FALLEN, FODDER_ARMY_SIZE + 1));
    CHECK(!fodder_profile_claim(&q, &p, 0, 3));
    CHECK(fodder_profile_result(&q, &p, FODDER_SURVIVED, 12));
    CHECK(q.pending && q.streak == 1 && q.best_streak == 1 && q.best_kills == 12);
    CHECK(seed != fodder_profile_seed(&q));
    CHECK(!fodder_profile_result(&r, &q, FODDER_SURVIVED, 12));
    CHECK(!fodder_profile_claim(&r, &q, 3, 3));
    CHECK(!fodder_profile_claim(&r, &q, 0, 0));
    CHECK(fodder_profile_claim(&r, &q, 1, 3));
    CHECK(!r.pending && r.variant == 1 && r.rank[1] == 1 && r.revision == 2);
    CHECK(fodder_profile_result(&q, &r, FODDER_FALLEN, 0));
    CHECK(q.streak == 0 && q.best_streak == 1 && q.rank[1] == 1 && q.best_kills == 12);
    CHECK(fodder_profile_encode(&q, encoded, sizeof(encoded)));
    CHECK(fodder_profile_decode(&p, encoded, 1) && p.variant == 0 && p.rank[1] == 1);
    CHECK(fodder_profile_decode(&p, encoded, 3) && p.variant == 1 && p.attempts == 2);
    q.revision = UINT_MAX;
    CHECK(!fodder_profile_result(&p, &q, FODDER_FALLEN, 0));
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
        "rules.upgrades[1] = true", "rules.effects = false",
        "rules.effects = function() return false end",
        "rules.effects = function() return { recovery=3, block_cost=1, dressing=0 } end"
    };
    for (unsigned i = 0; i < sizeof(mutations) / sizeof(mutations[0]); ++i) {
        wrap_rules(pack, mutations[i]);
        CHECK(!fodder_pack_open(&fixture_pack, test_root, error));
    }
    wrap_rules(pack, "local effects = rules.effects; rules.effects = function(a,b,c) "
                     "if a == 3 then error({}) end; return effects(a,b,c) end");
    CHECK(fodder_pack_open(&fixture_pack, test_root, error));
    CHECK(!error[0] && fixture_pack.variant_count == 3);
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
    fodder_mesh_free(&mesh);
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
    attacker->windup = 0.005f;
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
    for (unsigned i = 0; i < 14; ++i)
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
    CHECK(app.screen == FODDER_CAMP && app.profile.rank[1] == 1);
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
    test_session(&pack);
    fodder_pack_close(&pack);
    remove(fixture);
    REMOVE_DIR(test_root);
    puts("Fodder core unit tests passed.");
    return 0;
}
