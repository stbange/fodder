#include "render.h"
#include "sound.h"

#include <SDL3/SDL.h>
#ifndef __EMSCRIPTEN__
#include <SDL3/SDL_main.h>
#include "profile_store.h"
#else
#include <emscripten.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    SDL_Window *window;
    SDL_GLContext context;
    FodderRenderer *renderer;
    FodderScene *scene;
    FodderSound *sound;
    FodderPack *pack;
    FodderSession session;
    FodderInput input;
    char assets[FODDER_PATH];
    char error[FODDER_ERROR];
    bool running, audio;
} App;

static App app;

#ifndef __EMSCRIPTEN__
static void close_app(void)
{
    fodder_sound_free(app.sound);
    fodder_scene_free(app.renderer, app.scene);
    fodder_renderer_close(app.renderer);
    if (app.pack) {
        fodder_pack_close(app.pack);
        free(app.pack);
    }
    if (app.context)
        SDL_GL_DestroyContext(app.context);
    if (app.window)
        SDL_DestroyWindow(app.window);
    SDL_Quit();
}
#endif

#ifdef __EMSCRIPTEN__
EM_JS(void, browser_save, (const char *id, const char *text, unsigned revision), {
    Module.persistProfile(UTF8ToString(id), UTF8ToString(text), revision >>> 0);
})

EM_JS(void, browser_framebuffer, (int xr), {
    if (GLctx) GLctx.bindFramebuffer(GLctx.FRAMEBUFFER, xr ? Module.xrFramebuffer : null);
})
#endif

static FodderWrite write_profile(void *data, const FodderProfile *before, const FodderProfile *after)
{
    App *game = data;
    char text[FODDER_PROFILE_TEXT];
    if (!fodder_profile_encode(after, text, sizeof(text)))
        return FODDER_WRITE_FAILED;
#ifdef __EMSCRIPTEN__
    browser_save(game->pack->id, text, before->revision);
    return FODDER_WRITE_PENDING;
#else
    return fodder_profile_write_file(game->pack->id, game->pack->variant_count, before->revision, text) ?
           FODDER_WRITE_COMPLETE : FODDER_WRITE_FAILED;
#endif
}

static bool read_profile(const FodderPack *pack, const char *supplied, FodderProfile *profile)
{
#ifdef __EMSCRIPTEN__
    return !supplied[0] || fodder_profile_decode(profile, supplied, pack->variant_count);
#else
    (void)supplied;
    return fodder_profile_read_file(pack->id, pack->variant_count, profile);
#endif
}

static bool use_pack(const char *path, const char *expected, const char *saved)
{
    FodderPack *pack = calloc(1, sizeof(*pack));
    if (!pack) {
        snprintf(app.error, sizeof(app.error), "Could not allocate a content pack.");
        return false;
    }
    if (!fodder_pack_open(pack, path, app.error)) {
        free(pack);
        return false;
    }
    FodderProfile profile = {0};
    bool valid = strcmp(pack->id, expected) == 0 && read_profile(pack, saved, &profile);
    if (!valid)
        snprintf(app.error, sizeof(app.error), "Pack ID or saved progression is invalid.");
    FodderScene *scene = valid ? fodder_scene_load(app.renderer, pack, app.error) : NULL;
    FodderSound *sound = scene ? fodder_sound_load(pack, app.error) : NULL;
    if (!sound) {
        fodder_scene_free(app.renderer, scene);
        fodder_pack_close(pack);
        free(pack);
        return false;
    }
    fodder_sound_free(app.sound);
    fodder_scene_free(app.renderer, app.scene);
    if (app.pack) {
        fodder_pack_close(app.pack);
        free(app.pack);
    }
    app.scene = scene;
    app.pack = pack;
    app.sound = sound;
    app.input = (FodderInput){0};
    fodder_session_open(&app.session, pack, profile, write_profile, &app);
    if (app.audio)
        fodder_sound_enable(sound);
    app.error[0] = '\0';
    return true;
}

static bool boot(const char *id, const char *saved, bool smoke)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        snprintf(app.error, sizeof(app.error), "SDL video initialization failed. %s", SDL_GetError());
        return false;
    }
#ifdef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    const char *base = "/content";
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    const char *base = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "FODDER_ASSETS");
    if (!base) {
        const char *directory = SDL_GetBasePath();
        if (!directory || !fodder_path_join(app.assets, sizeof(app.assets), directory, "assets")) {
            snprintf(app.error, sizeof(app.error), "Could not locate assets beside the game.");
            return false;
        }
    }
#endif
    if (base)
        snprintf(app.assets, sizeof(app.assets), "%s", base);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (smoke)
        flags |= SDL_WINDOW_HIDDEN;
    app.window = SDL_CreateWindow("Fodder", 960, 540, flags);
    app.context = app.window ? SDL_GL_CreateContext(app.window) : NULL;
    if (!app.context) {
        snprintf(app.error, sizeof(app.error), "OpenGL context creation failed. %s", SDL_GetError());
        return false;
    }
#ifndef __EMSCRIPTEN__
    (void)SDL_GL_SetSwapInterval(smoke ? 0 : 1);
#endif
    app.renderer = fodder_renderer_open(app.assets, app.error);
    char relative[80], pack_path[FODDER_PATH];
    snprintf(relative, sizeof(relative), "packs/%s", id);
    app.running = app.renderer && fodder_path_join(pack_path, sizeof(pack_path), app.assets, relative) &&
                  use_pack(pack_path, id, saved);
    return app.running;
}

static void audio_enable(void)
{
    app.audio = true;
    fodder_sound_enable(app.sound);
}

static void action(FodderAction command)
{
    FodderScreen before = app.session.screen;
    fodder_session_action(&app.session, command);
    if (before != app.session.screen) {
        if (app.session.screen == FODDER_FIGHT)
            audio_enable();
        app.input = (FodderInput){0};
    }
#ifndef __EMSCRIPTEN__
    if (before != app.session.screen)
        SDL_SetWindowRelativeMouseMode(app.window, app.session.screen == FODDER_FIGHT);
#endif
}

static void step(double delta)
{
    FodderScreen before = app.session.screen;
    fodder_session_frame(&app.session, delta, app.input);
    app.input.look_x = app.input.look_y = 0;
    app.input.attack_pressed = false;
    if (before != app.session.screen) {
        app.input = (FodderInput){0};
#ifndef __EMSCRIPTEN__
        SDL_SetWindowRelativeMouseMode(app.window, false);
#endif
    }
    fodder_sound_frame(app.sound, &app.session);
}

static void flat_frame(int w, int h, bool web)
{
    fodder_screen_draw(app.renderer, app.scene, &app.session, w, h, web);
    SDL_GL_SwapWindow(app.window);
}

#ifdef __EMSCRIPTEN__
static float xr_matrices[36];

EMSCRIPTEN_KEEPALIVE int fodder_boot(const char *profile)
{
    return boot("fantasy", profile, false);
}

EMSCRIPTEN_KEEPALIVE const char *fodder_error(void)
{
    return app.error;
}

EMSCRIPTEN_KEEPALIVE void fodder_save_done(int success)
{
    fodder_session_saved(&app.session, success != 0);
}

EMSCRIPTEN_KEEPALIVE int fodder_load_pack(const char *path, const char *id, const char *profile)
{
    if (!app.renderer) {
        snprintf(app.error, sizeof(app.error), "Wait for the graphics context to recover.");
        return 0;
    }
    if (app.session.screen == FODDER_FIGHT || app.session.saving || app.session.save_error) {
        snprintf(app.error, sizeof(app.error), "Finish the current battle or save before changing packs.");
        return 0;
    }
    return use_pack(path, id, profile);
}

EMSCRIPTEN_KEEPALIVE void fodder_command(int command)
{
    if (command >= FODDER_CONFIRM && command <= FODDER_THIRD_UPGRADE)
        action((FodderAction)command);
}

EMSCRIPTEN_KEEPALIVE void fodder_control(float move, float strafe, float look_x, float look_y,
                                         int attack, int block)
{
    app.input = (FodderInput){
        .move = move, .strafe = strafe, .look_x = look_x, .look_y = look_y,
        .attack = attack != 0, .block = block != 0
    };
}

EMSCRIPTEN_KEEPALIVE void fodder_clear_input(void)
{
    app.input = (FodderInput){0};
    fodder_session_clear_input(&app.session);
}

EMSCRIPTEN_KEEPALIVE void fodder_flat(double delta, int width, int height)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
    }
    step(delta);
    browser_framebuffer(0);
    flat_frame(width, height, true);
}

EMSCRIPTEN_KEEPALIVE void fodder_drop_graphics(void)
{
    fodder_scene_free(app.renderer, app.scene);
    fodder_renderer_close(app.renderer);
    app.scene = NULL;
    app.renderer = NULL;
}

EMSCRIPTEN_KEEPALIVE int fodder_restore_graphics(void)
{
    if (app.renderer)
        return app.scene != NULL;
    app.renderer = fodder_renderer_open(app.assets, app.error);
    app.scene = app.renderer ? fodder_scene_load(app.renderer, app.pack, app.error) : NULL;
    return app.scene != NULL;
}

EMSCRIPTEN_KEEPALIVE void fodder_xr_step(double delta, float yaw)
{
    app.session.battle.soldiers[0].yaw = fodder_wrap(yaw);
    step(delta);
}

EMSCRIPTEN_KEEPALIVE float *fodder_xr_buffer(void)
{
    return xr_matrices;
}

EMSCRIPTEN_KEEPALIVE void fodder_xr_view(int x, int y, int width, int height)
{
    browser_framebuffer(1);
    fodder_eye_draw(app.renderer, app.scene, &app.session, xr_matrices, x, y, width, height);
}

EMSCRIPTEN_KEEPALIVE float fodder_world(int axis)
{
    const FodderSoldier *player = app.session.battle.soldiers;
    if (axis == 0)
        return player->x;
    if (axis == 1)
        return fodder_ground(app.session.battle.arena, player->x, player->z);
    if (axis == 2)
        return player->z;
    return player->yaw;
}

EMSCRIPTEN_KEEPALIVE unsigned fodder_stat(int index)
{
    const FodderSession *s = &app.session;
    switch (index) {
    case 0: return (unsigned)s->screen;
    case 1: return s->battle.alive[0];
    case 2: return s->battle.alive[1];
    case 3: return s->battle.kills;
    case 4: return fodder_time_left(&s->battle);
    case 5: return s->profile.best_kills;
    case 6: return s->profile.streak;
    case 7: return s->profile.best_streak;
    case 8: return s->profile.last_kills;
    case 9: return s->saving ? 1u : s->save_error ? 2u : 0u;
    case 10: return s->profile.rank[0];
    case 11: return s->profile.rank[1];
    case 12: return s->profile.rank[2];
    case 13: return (unsigned)(s->battle.winner + 1);
    case 14: return (unsigned)s->battle.end;
    case 15: return (unsigned)s->battle.soldiers[0].health;
    case 16: return s->selection;
    case 17: return s->battle.tick;
    case 18: return fodder_profile_reward(&s->profile);
    default: return 0;
    }
}

EMSCRIPTEN_KEEPALIVE const char *fodder_label(int type, int index)
{
    const FodderSession *s = &app.session;
    switch (type) {
    case 0: return s->pack->title;
    case 1: return s->battle.variant.name;
    case 2: return s->battle.variant.hint;
    case 3: return index >= 0 && index < FODDER_UPGRADES ? s->pack->upgrades[index].name : "";
    case 4: return index >= 0 && index < FODDER_UPGRADES ? s->pack->upgrades[index].description : "";
    case 5: return s->pack->id;
    default: return "";
    }
}

int main(void)
{
    return 0;
}
#else
static void keyboard(SDL_KeyboardEvent event)
{
    if (event.repeat || !event.down)
        return;
    switch (event.scancode) {
    case SDL_SCANCODE_ESCAPE: action(FODDER_BACK); break;
    case SDL_SCANCODE_SPACE:
    case SDL_SCANCODE_RETURN: action(FODDER_CONFIRM); break;
    case SDL_SCANCODE_UP:
    case SDL_SCANCODE_LEFT: action(FODDER_PREVIOUS); break;
    case SDL_SCANCODE_DOWN:
    case SDL_SCANCODE_RIGHT: action(FODDER_NEXT); break;
    case SDL_SCANCODE_1: action(FODDER_FIRST_UPGRADE); break;
    case SDL_SCANCODE_2: action(FODDER_SECOND_UPGRADE); break;
    case SDL_SCANCODE_3: action(FODDER_THIRD_UPGRADE); break;
    default: break;
    }
}

static void events(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            app.running = false;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (app.session.screen == FODDER_FIGHT)
                action(FODDER_BACK);
            break;
        case SDL_EVENT_KEY_DOWN:
            keyboard(event.key);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT && app.session.screen == FODDER_FIGHT)
                app.input.attack_pressed = true;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (app.session.screen == FODDER_FIGHT) {
                app.input.look_x += event.motion.xrel * 0.00235f;
                app.input.look_y -= event.motion.yrel * 0.00235f;
            }
            break;
        default: break;
        }
    }
    const bool *keys = SDL_GetKeyboardState(NULL);
    SDL_MouseButtonFlags buttons = SDL_GetMouseState(NULL, NULL);
    app.input.move = (float)keys[SDL_SCANCODE_W] - (float)keys[SDL_SCANCODE_S];
    app.input.strafe = (float)keys[SDL_SCANCODE_D] - (float)keys[SDL_SCANCODE_A];
    app.input.attack = (buttons & SDL_BUTTON_LMASK) != 0;
    app.input.block = (buttons & SDL_BUTTON_RMASK) != 0;
}

int main(int argc, char *argv[])
{
    const char *id = "fantasy";
    bool smoke = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--smoke") == 0)
            smoke = true;
        else if (strcmp(argv[i], "--pack") == 0 && i + 1 < argc)
            id = argv[++i];
        else {
            fprintf(stderr, "Usage: fodder [--pack id] [--smoke]\n");
            return 2;
        }
    }
    if (!fodder_valid_id(id) || !boot(id, "", smoke)) {
        fprintf(stderr, "%s\n", app.error[0] ? app.error : "Invalid content pack ID.");
        close_app();
        return 1;
    }

    Uint64 last = SDL_GetTicksNS();
    unsigned frames = 0;
    while (app.running) {
        if (!smoke)
            events();
        Uint64 now = SDL_GetTicksNS();
        step((double)(now - last) / 1000000000.0);
        last = now;
        int w, h;
        SDL_GetWindowSizeInPixels(app.window, &w, &h);
        flat_frame(w, h, false);
        if (smoke && ++frames == 3)
            app.running = false;
    }
    bool valid = !smoke || fodder_renderer_check(app.renderer);
    if (smoke && valid)
        printf("OpenGL rendered three frames, loaded %u soldiers from %s.\n", app.session.battle.count, app.pack->id);
    close_app();
    return valid ? 0 : 1;
}
#endif
