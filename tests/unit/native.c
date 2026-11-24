#define SDL_MAIN_HANDLED
#define main fodder_program
#include "main.c"
#undef main

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #test, SDL_GetError()); exit(1); } } while (0)

static void scratch_profile(const char *id, const char *suffix, char path[FODDER_PATH])
{
    char *directory = SDL_GetPrefPath("Fodder", "Fodder");
    CHECK(directory);
    char name[80];
    SDL_snprintf(name, sizeof(name), "%s.%s", id, suffix);
    CHECK(fodder_path_join(path, FODDER_PATH, directory, name));
    SDL_free(directory);
}

static void test_file_profile(void)
{
    FodderProfile empty = {0}, saved = {0};
    char path[FODDER_PATH], lock[FODDER_PATH], text[FODDER_PROFILE_TEXT];
    scratch_profile("component-test", "profile", path);
    scratch_profile("component-test", "lock", lock);
    (void)SDL_RemovePath(path);
    CHECK(fodder_profile_read_file("component-test", 3, &saved) && saved.revision == 0);
    CHECK(fodder_profile_encode(&empty, text, sizeof(text)));
    int fd = open(lock, O_CREAT | O_WRONLY, 0600);
    CHECK(fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) == 0);
    CHECK(!fodder_profile_write_file("component-test", 3, 0, text));
    CHECK(close(fd) == 0);
    FodderProfile earned = {.revision = 1, .rank = {1, 0, 0}};
    CHECK(fodder_profile_encode(&earned, text, sizeof(text)));
    CHECK(fodder_profile_write_file("component-test", 3, 0, text));
    CHECK(fodder_profile_read_file("component-test", 3, &saved) && saved.rank[0] == 1);
    CHECK(!fodder_profile_write_file("component-test", 3, 0, text));
    CHECK(SDL_SaveFile(path, "damaged", 7));
    CHECK(!fodder_profile_read_file("component-test", 3, &saved));
    CHECK(!fodder_profile_write_file("component-test", 3, 1, text));
    CHECK(SDL_RemovePath(path));
    CHECK(SDL_CreateDirectory(path));
    CHECK(!fodder_profile_read_file("component-test", 3, &saved));
    CHECK(SDL_RemovePath(path));
    CHECK(SDL_RemovePath(lock));
}

static void test_shaders(const char *assets)
{
    const char *root = "platform-unit";
    char error[FODDER_ERROR], source[FODDER_PATH], target[FODDER_PATH];
    CHECK(!fodder_renderer_open("platform-unit/missing", error));
    CHECK(SDL_CreateDirectory("platform-unit/ui"));
    CHECK(SDL_CreateDirectory("platform-unit/shaders"));
    CHECK(fodder_path_join(source, sizeof(source), assets, "ui/font.txt"));
    CHECK(SDL_CopyFile(source, "platform-unit/ui/font.txt"));
    CHECK(!fodder_renderer_open(root, error));
    const char *invalid = "void main() { invalid tokens }";
    CHECK(SDL_SaveFile("platform-unit/shaders/world.vert", invalid, strlen(invalid)));
    CHECK(!fodder_renderer_open(root, error));
    CHECK(fodder_path_join(source, sizeof(source), assets, "shaders/world.vert"));
    CHECK(SDL_CopyFile(source, "platform-unit/shaders/world.vert"));
    CHECK(!fodder_renderer_open(root, error));
    const char *incompatible = "in vec3 vColor; out vec4 fragmentColor; void main(){fragmentColor=vec4(vColor,1.0);}";
    CHECK(SDL_SaveFile("platform-unit/shaders/world.frag", incompatible, strlen(incompatible)));
    CHECK(!fodder_renderer_open(root, error));
    CHECK(fodder_path_join(source, sizeof(source), assets, "shaders/world.frag"));
    CHECK(SDL_CopyFile(source, "platform-unit/shaders/world.frag"));
    FodderRenderer *second = fodder_renderer_open(root, error);
    CHECK(second);
    fodder_renderer_close(second);
    const char *files[] = { "ui/font.txt", "shaders/world.vert", "shaders/world.frag", "ui", "shaders" };
    for (unsigned i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        CHECK(fodder_path_join(target, sizeof(target), root, files[i]));
        CHECK(SDL_RemovePath(target));
    }
    CHECK(SDL_RemovePath(root));
}

static void send_key(SDL_Scancode code)
{
    SDL_Event event = {0};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.down = true;
    event.key.scancode = code;
    CHECK(SDL_PushEvent(&event));
    events();
}

static void test_game_components(const char *assets)
{
    char path[FODDER_PATH], error[FODDER_ERROR];
    scratch_profile("fantasy", "profile", path);
    (void)SDL_RemovePath(path);
    bool ready = boot("fantasy", "", true);
    if (!ready)
        fprintf(stderr, "%s\n", app.error);
    CHECK(ready);
    CHECK(app.session.battle.count == 1000 && app.session.screen == FODDER_CAMP);
    CHECK(!use_pack("missing", "fantasy", ""));
    char standard[FODDER_PATH];
    CHECK(fodder_path_join(standard, sizeof(standard), assets, "packs/fantasy"));
    CHECK(!use_pack(standard, "wrong-id", ""));
    test_file_profile();
    test_shaders(assets);
    // The extra renderer changed global GL bindings. Each frame binds its own objects.
    fodder_screen_draw(app.renderer, app.scene, &app.session, 0, 540, false);
    flat_frame(960, 540, false);
    FodderPack changed = *app.pack;
    strcpy(changed.files[FODDER_SOLDIER_MESH], "missing");
    CHECK(!fodder_scene_load(app.renderer, &changed, error));
    changed = *app.pack;
    strcpy(changed.files[FODDER_SPRITE], "missing");
    CHECK(!fodder_scene_load(app.renderer, &changed, error));
    strcpy(changed.files[FODDER_STRIKE_AUDIO], "missing");
    CHECK(!fodder_sound_load(&changed, error));
    changed = *app.pack;
    for (unsigned i = FODDER_SWING_AUDIO; i < FODDER_PACK_FILES; ++i)
        changed.files[i][0] = '\0';
    FodderSound *legacy = fodder_sound_load(&changed, error);
    CHECK(legacy);
    fodder_sound_free(legacy);

    keyboard((SDL_KeyboardEvent){.down = false});
    send_key(SDL_SCANCODE_Q);
    send_key(SDL_SCANCODE_RETURN);
    CHECK(app.session.screen == FODDER_FIGHT);
    fodder_sound_enable(app.sound);
    send_key(SDL_SCANCODE_ESCAPE);
    CHECK(app.session.screen == FODDER_PAUSED);
    flat_frame(960, 540, false);
    send_key(SDL_SCANCODE_SPACE);
    CHECK(app.session.screen == FODDER_FIGHT);
    SDL_Event event = {0};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.xrel = 81;
    CHECK(SDL_PushEvent(&event));
    events();
    step(0.03);
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    CHECK(SDL_PushEvent(&event));
    events();
    CHECK(app.input.attack_pressed);
    app.input.attack = true;
    app.session.battle.soldiers[1].x = 180;
    app.session.battle.soldiers[2].health = 0;
    FodderSoldier *attacker = &app.session.battle.soldiers[FODDER_ARMY_SIZE];
    attacker->windup = 0.2f;
    attacker->windup_duration = 0.65f;
    attacker->strike = FODDER_STRIKE_OVERHEAD;
    attacker->x = 1.4f;
    attacker->z = 15.0f;
    attacker[1].x = 0.4f;
    attacker[1].z = 14.0f;
    attacker[1].stagger = 0.3f;
    attacker[2].x = -0.3f;
    attacker[2].z = 19.0f;
    app.session.battle.attacker = FODDER_ARMY_SIZE;
    app.session.battle.flanker = FODDER_ARMY_SIZE + 2;
    app.session.battle.counter_target = FODDER_ARMY_SIZE + 1;
    app.session.battle.counter_time = 0.5f;
    app.session.battle.hurt = 0.6f;
    fodder_sound_frame(app.sound, &app.session);
    flat_frame(960, 540, false);
    step(0.036);
    app.input.attack = false;
    app.input.block = true;
    step(0.1);
    step(0.15);
    CHECK(app.session.battle.soldiers[0].recovery > 0.0f);

    FodderMatrix view = fodder_identity();
    float matrices[36] = {0};
    memcpy(matrices, view.data, sizeof(view.data));
    memcpy(matrices + 16, view.data, sizeof(view.data));
    fodder_eye_draw(app.renderer, app.scene, &app.session, matrices, 0, 0, 0, 540);
    fodder_eye_draw(app.renderer, app.scene, &app.session, matrices, 0, 0, 480, 540);

    app.session.battle.variant.seconds = 1;
    app.session.battle.tick = 59;
    step(0.04);
    CHECK(app.session.screen == FODDER_REWARD);
    flat_frame(960, 540, false);
    CHECK(app.session.profile.pending);
    send_key(SDL_SCANCODE_UP);
    send_key(SDL_SCANCODE_DOWN);
    send_key(SDL_SCANCODE_2);
    CHECK(app.session.profile.rank[1] == 1 && app.session.screen == FODDER_CAMP);
    FodderProfile after = {0};
    CHECK(fodder_profile_read_file("fantasy", 3, &after) && after.rank[1] == 1);
    send_key(SDL_SCANCODE_1);
    send_key(SDL_SCANCODE_3);
    send_key(SDL_SCANCODE_RETURN);
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    CHECK(SDL_PushEvent(&event));
    events();
    CHECK(app.session.screen == FODDER_PAUSED);
    action(FODDER_CONFIRM);
    app.session.battle.soldiers[0].health = 0;
    step(0.04);
    CHECK(app.session.screen == FODDER_RESULT);
    flat_frame(960, 540, false);
    app.session.save_error = true;
    flat_frame(960, 540, false);
    app.session.save_error = false;
    app.session.saving = true;
    flat_frame(960, 540, false);
    app.session.saving = false;
    CHECK(fodder_renderer_check(app.renderer));
    event.type = SDL_EVENT_QUIT;
    CHECK(SDL_PushEvent(&event));
    events();
    CHECK(!app.running);
    close_app();
    memset(&app, 0, sizeof(app));
    CHECK(SDL_RemovePath(path));
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    SDL_Environment *env = SDL_GetEnvironment();
    CHECK(SDL_SetEnvironmentVariable(env, "FODDER_ASSETS", "missing-assets", true));
    CHECK(!boot("fantasy", "", true));
    close_app();
    memset(&app, 0, sizeof(app));
    CHECK(SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "FODDER_ASSETS", argv[1], true));
    char *invalid[] = {argv[0], "--unknown"};
    CHECK(fodder_program(2, invalid) == 2);
    char *wrong_id[] = {argv[0], "--pack", "BAD ID"};
    CHECK(fodder_program(3, wrong_id) == 1);
    CHECK(SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "FODDER_ASSETS", argv[1], true));
    char *smoke[] = {argv[0], "--smoke"};
    CHECK(fodder_program(2, smoke) == 0);
    memset(&app, 0, sizeof(app));
    CHECK(SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "FODDER_ASSETS", argv[1], true));
    test_game_components(argv[1]);
    puts("Native renderer, input, sound and profile components passed.");
    return 0;
}
