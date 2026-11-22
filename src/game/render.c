#include "render.h"

#include <SDL3/SDL.h>
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#define FODDER_GL_CALL
#else
#include <SDL3/SDL_opengl.h>
#define FODDER_GL_CALL APIENTRY
#endif

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FODDER_GL_FUNCTIONS(F) \
    F(GLuint, CreateShader, (GLenum type)) \
    F(void, ShaderSource, (GLuint shader, GLsizei n, const GLchar *const *src, const GLint *len)) \
    F(void, CompileShader, (GLuint shader)) \
    F(void, GetShaderiv, (GLuint shader, GLenum key, GLint *value)) \
    F(void, GetShaderInfoLog, (GLuint shader, GLsizei n, GLsizei *len, GLchar *log)) \
    F(void, DeleteShader, (GLuint shader)) \
    F(GLuint, CreateProgram, (void)) \
    F(void, AttachShader, (GLuint program, GLuint shader)) \
    F(void, LinkProgram, (GLuint program)) \
    F(void, GetProgramiv, (GLuint program, GLenum name, GLint *value)) \
    F(void, GetProgramInfoLog, (GLuint program, GLsizei n, GLsizei *len, GLchar *log)) \
    F(void, DeleteProgram, (GLuint program)) \
    F(void, UseProgram, (GLuint program)) \
    F(GLint, GetUniformLocation, (GLuint program, const GLchar *name)) \
    F(void, Uniform1i, (GLint location, GLint value)) \
    F(void, Uniform1f, (GLint location, GLfloat value)) \
    F(void, Uniform3f, (GLint location, GLfloat x, GLfloat y, GLfloat z)) \
    F(void, UniformMatrix4fv, (GLint location, GLsizei n, GLboolean transpose, const GLfloat *value)) \
    F(void, GenVertexArrays, (GLsizei count, GLuint *arrays)) \
    F(void, BindVertexArray, (GLuint vao)) \
    F(void, DeleteVertexArrays, (GLsizei count, const GLuint *arrays)) \
    F(void, GenBuffers, (GLsizei count, GLuint *buffers)) \
    F(void, BindBuffer, (GLenum target, GLuint buffer)) \
    F(void, BufferData, (GLenum target, GLsizeiptr size, const void *data, GLenum usage)) \
    F(void, DeleteBuffers, (GLsizei count, const GLuint *buffers)) \
    F(void, EnableVertexAttribArray, (GLuint index)) \
    F(void, VertexAttribPointer, (GLuint index, GLint n, GLenum type, GLboolean norm, GLsizei stride, const void *ptr)) \
    F(void, VertexAttribDivisor, (GLuint index, GLuint divisor)) \
    F(void, DrawArraysInstanced, (GLenum mode, GLint first, GLsizei count, GLsizei instances)) \
    F(void, GenTextures, (GLsizei n, GLuint *textures)) \
    F(void, BindTexture, (GLenum target, GLuint texture)) \
    F(void, TexParameteri, (GLenum target, GLenum key, GLint value)) \
    F(void, TexImage2D, (GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum fmt, GLenum type, const void *data)) \
    F(void, DeleteTextures, (GLsizei n, const GLuint *textures)) \
    F(void, Viewport, (GLint x, GLint y, GLsizei w, GLsizei h)) \
    F(void, Scissor, (GLint x, GLint y, GLsizei w, GLsizei h)) \
    F(void, Enable, (GLenum feature)) \
    F(void, Disable, (GLenum feature)) \
    F(void, Clear, (GLbitfield mask)) \
    F(void, ClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a)) \
    F(void, BlendFunc, (GLenum source, GLenum destination)) \
    F(GLenum, GetError, (void))

typedef struct {
#define GL_DECLARE(result, name, args) result (FODDER_GL_CALL *name) args;
    FODDER_GL_FUNCTIONS(GL_DECLARE)
#undef GL_DECLARE
} FodderGL;

typedef struct {
    float x, y, z, yaw;
    FodderColor dye;
} Instance;

typedef struct {
    GLuint vao, vertices;
    GLsizei count;
} Drawable;

struct FodderRenderer {
    FodderGL gl;
    GLuint program, instances;
    int camera, model, eye, style, fog, haze;
    Drawable overlay, billboard;
    FodderMesh rectangles;
    unsigned char font[127][7];
    Instance close[FODDER_SOLDIERS], far[FODDER_SOLDIERS];
};

struct FodderScene {
    Drawable world, levy, sword, shield, pennants;
    GLuint sprite, banner;
};

static const FodderColor colors[2] = {{0.22f, 0.77f, 0.61f, 1}, {0.98f, 0.40f, 0.23f, 1}};
static const FodderColor ivory = {0.94f, 0.88f, 0.74f, 1};
static const FodderColor gold = {0.95f, 0.67f, 0.34f, 1};
static const FodderColor gray = {0.72f, 0.78f, 0.74f, 1};

static bool load_gl(FodderGL *gl)
{
#ifdef __EMSCRIPTEN__
#define FODDER_GL_LOAD(result, name, args) gl->name = gl##name;
#else
#define FODDER_GL_LOAD(result, name, args) \
    gl->name = (result (FODDER_GL_CALL *) args)SDL_GL_GetProcAddress("gl" #name); \
    if (!gl->name) return false;
#endif
    FODDER_GL_FUNCTIONS(FODDER_GL_LOAD)
#undef FODDER_GL_LOAD
    return true;
}

static GLuint shader(FodderGL *gl, const char *root, const char *name, GLenum kind,
                      char error[FODDER_ERROR])
{
    char path[FODDER_PATH];
    size_t length;
    char *code = fodder_path_join(path, sizeof(path), root, name) ? fodder_read(path, 65536, &length) : NULL;
    if (!code) {
        snprintf(error, FODDER_ERROR, "Could not open %s.", name);
        return 0;
    }
#ifdef __EMSCRIPTEN__
    const char *version = "#version 300 es\nprecision mediump float;\nprecision mediump int;\n";
#else
    const char *version = "#version 330 core\n";
#endif
    const char *sources[] = {version, code};
    GLuint handle = gl->CreateShader(kind);
    gl->ShaderSource(handle, 2, sources, NULL);
    gl->CompileShader(handle);
    free(code);
    GLint status;
    gl->GetShaderiv(handle, GL_COMPILE_STATUS, &status);
    if (!status) {
        gl->GetShaderInfoLog(handle, FODDER_ERROR, NULL, error);
        gl->DeleteShader(handle);
        return 0;
    }
    return handle;
}

static GLuint program(FodderGL *gl, const char *root, char error[FODDER_ERROR])
{
    GLuint vertex = shader(gl, root, "shaders/world.vert", GL_VERTEX_SHADER, error);
    if (!vertex)
        return 0;
    GLuint fragment = shader(gl, root, "shaders/world.frag", GL_FRAGMENT_SHADER, error);
    GLuint handle = 0;
    if (fragment) {
        handle = gl->CreateProgram();
        gl->AttachShader(handle, vertex);
        gl->AttachShader(handle, fragment);
        gl->LinkProgram(handle);
        gl->DeleteShader(fragment);
        GLint status;
        gl->GetProgramiv(handle, GL_LINK_STATUS, &status);
        if (!status) {
            gl->GetProgramInfoLog(handle, FODDER_ERROR, NULL, error);
            gl->DeleteProgram(handle);
            handle = 0;
        }
    }
    gl->DeleteShader(vertex);
    return handle;
}

static void bind_vertices(FodderRenderer *r, Drawable *buffer)
{
    FodderGL *gl = &r->gl;
    gl->GenVertexArrays(1, &buffer->vao);
    gl->GenBuffers(1, &buffer->vertices);
    gl->BindVertexArray(buffer->vao);
    gl->BindBuffer(GL_ARRAY_BUFFER, buffer->vertices);
    gl->EnableVertexAttribArray(0);
    gl->VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FodderVertex), NULL);
    gl->EnableVertexAttribArray(1);
    gl->VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FodderVertex), (void *)offsetof(FodderVertex, u));
    gl->EnableVertexAttribArray(2);
    gl->VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(FodderVertex), (void *)offsetof(FodderVertex, color));
    gl->BindBuffer(GL_ARRAY_BUFFER, r->instances);
    gl->EnableVertexAttribArray(3);
    gl->VertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), NULL);
    gl->VertexAttribDivisor(3, 1);
    gl->EnableVertexAttribArray(4);
    gl->VertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void *)offsetof(Instance, dye));
    gl->VertexAttribDivisor(4, 1);
    gl->BindVertexArray(0);
}

static void upload(FodderRenderer *r, Drawable *buffer, const FodderMesh *mesh, GLenum usage)
{
    r->gl.BindBuffer(GL_ARRAY_BUFFER, buffer->vertices);
    r->gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(mesh->count * sizeof(FodderVertex)), mesh->vertices, usage);
    buffer->count = (GLsizei)mesh->count;
}

static void free_drawable(FodderGL *gl, Drawable *buffer)
{
    gl->DeleteVertexArrays(1, &buffer->vao);
    gl->DeleteBuffers(1, &buffer->vertices);
}

FodderRenderer *fodder_renderer_open(const char *assets, char error[FODDER_ERROR])
{
    FodderRenderer *r = calloc(1, sizeof(*r));
    if (!r || !load_gl(&r->gl)) {
        free(r);
        snprintf(error, FODDER_ERROR, "OpenGL 3.3 or WebGL 2 is required.");
        return NULL;
    }
    char path[FODDER_PATH];
    if (!fodder_path_join(path, sizeof(path), assets, "ui/font.txt") || !fodder_font_read(r->font, path)) {
        snprintf(error, FODDER_ERROR, "Could not read the UI font.");
        free(r);
        return NULL;
    }
    FodderGL *gl = &r->gl;
    r->program = program(gl, assets, error);
    if (!r->program) {
        free(r);
        return NULL;
    }
    gl->UseProgram(r->program);
    r->camera = gl->GetUniformLocation(r->program, "uCamera");
    r->model = gl->GetUniformLocation(r->program, "uModel");
    r->eye = gl->GetUniformLocation(r->program, "uEye");
    r->style = gl->GetUniformLocation(r->program, "uStyle");
    r->fog = gl->GetUniformLocation(r->program, "uFog");
    r->haze = gl->GetUniformLocation(r->program, "uHaze");
    gl->Uniform1i(gl->GetUniformLocation(r->program, "uTexture"), 0);
    gl->GenBuffers(1, &r->instances);
    bind_vertices(r, &r->overlay);
    bind_vertices(r, &r->billboard);
    FodderMesh plane = {0};
    bool success = fodder_quad(&plane, (FodderVec){-0.63f, 0, 0}, (FodderVec){0.63f, 0, 0},
                               (FodderVec){0.63f, 2.05f, 0}, (FodderVec){-0.63f, 2.05f, 0}, ivory);
    if (success)
        upload(r, &r->billboard, &plane, GL_STATIC_DRAW);
    fodder_mesh_free(&plane);
    if (!success) {
        snprintf(error, FODDER_ERROR, "Unable to allocate sprite geometry.");
        fodder_renderer_close(r);
        return NULL;
    }
    gl->BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return r;
}

void fodder_renderer_close(FodderRenderer *r)
{
    if (!r)
        return;
    free_drawable(&r->gl, &r->overlay);
    free_drawable(&r->gl, &r->billboard);
    r->gl.DeleteBuffers(1, &r->instances);
    r->gl.DeleteProgram(r->program);
    fodder_mesh_free(&r->rectangles);
    free(r);
}

static bool load_mesh(FodderRenderer *r, const FodderPack *pack, Drawable *buffer, FodderPackFile file)
{
    char path[FODDER_PATH];
    FodderMesh mesh = {0};
    if (!fodder_pack_file(pack, file, path) || !fodder_mesh_read(&mesh, path))
        return false;
    bind_vertices(r, buffer);
    upload(r, buffer, &mesh, GL_STATIC_DRAW);
    fodder_mesh_free(&mesh);
    return true;
}

static GLuint load_texture(FodderGL *gl, const FodderPack *pack, FodderPackFile file)
{
    char path[FODDER_PATH];
    FodderImage image = {0};
    if (!fodder_pack_file(pack, file, path) || !fodder_image_read(&image, path))
        return 0;
    GLuint texture;
    gl->GenTextures(1, &texture);
    gl->BindTexture(GL_TEXTURE_2D, texture);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)image.width, (GLsizei)image.height, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, image.pixels);
    fodder_image_free(&image);
    return texture;
}

FodderScene *fodder_scene_load(FodderRenderer *r, const FodderPack *pack, char error[FODDER_ERROR])
{
    FodderScene *scene = calloc(1, sizeof(*scene));
    if (!scene) {
        snprintf(error, FODDER_ERROR, "Could not allocate scene.");
        return NULL;
    }
    FodderMesh world = {0}, banners = {0};
    bool valid = fodder_landscape(&world, &pack->arena) && fodder_banners(&banners, &pack->arena) &&
                 load_mesh(r, pack, &scene->levy, FODDER_SOLDIER_MESH) &&
                 load_mesh(r, pack, &scene->sword, FODDER_SWORD_MESH) &&
                 load_mesh(r, pack, &scene->shield, FODDER_SHIELD_MESH);
    if (valid) {
        scene->sprite = load_texture(&r->gl, pack, FODDER_SPRITE);
        scene->banner = load_texture(&r->gl, pack, FODDER_BANNER);
        bind_vertices(r, &scene->world);
        upload(r, &scene->world, &world, GL_STATIC_DRAW);
        bind_vertices(r, &scene->pennants);
        upload(r, &scene->pennants, &banners, GL_STATIC_DRAW);
        valid = scene->sprite && scene->banner && fodder_renderer_check(r);
    }
    fodder_mesh_free(&world);
    fodder_mesh_free(&banners);
    if (valid)
        return scene;
    snprintf(error, FODDER_ERROR, "Could not open a model, sprite or banner texture.");
    fodder_scene_free(r, scene);
    return NULL;
}

void fodder_scene_free(FodderRenderer *r, FodderScene *scene)
{
    if (!scene)
        return;
    free_drawable(&r->gl, &scene->world);
    free_drawable(&r->gl, &scene->levy);
    free_drawable(&r->gl, &scene->sword);
    free_drawable(&r->gl, &scene->shield);
    free_drawable(&r->gl, &scene->pennants);
    r->gl.DeleteTextures(1, &scene->sprite);
    r->gl.DeleteTextures(1, &scene->banner);
    free(scene);
}

static void draw(FodderRenderer *r, const Drawable *d, int style, GLuint texture,
                 FodderMatrix model, const Instance *instances, unsigned count)
{
    if (!count || !d->count)
        return;
    FodderGL *gl = &r->gl;
    gl->UniformMatrix4fv(r->model, 1, GL_FALSE, model.data);
    gl->Uniform1i(r->style, style);
    gl->BindTexture(GL_TEXTURE_2D, texture);
    gl->BindBuffer(GL_ARRAY_BUFFER, r->instances);
    gl->BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(*instances)), instances, GL_STREAM_DRAW);
    gl->BindVertexArray(d->vao);
    gl->DrawArraysInstanced(GL_TRIANGLES, 0, d->count, (GLsizei)count);
}

static void overlay_draw(FodderRenderer *r)
{
    Instance neutral = {0, 0, 0, 0, {1, 1, 1, 1}};
    FodderMatrix identity = fodder_identity();
    r->gl.UniformMatrix4fv(r->camera, 1, GL_FALSE, identity.data);
    upload(r, &r->overlay, &r->rectangles, GL_STREAM_DRAW);
    draw(r, &r->overlay, 3, 0, identity, &neutral, 1);
    r->rectangles.count = 0;
}

static void rectangle(FodderRenderer *r, float x, float y, float w, float h, FodderColor color)
{
    float l = x / 480.0f - 1.0f, t = 1.0f - y / 270.0f;
    float right = l + w / 480.0f, bottom = t - h / 270.0f;
    (void)fodder_quad(&r->rectangles, (FodderVec){l, bottom, 0}, (FodderVec){right, bottom, 0},
                      (FodderVec){right, t, 0}, (FodderVec){l, t, 0}, color);
}

static void text(FodderRenderer *r, const char *message, float x, float y, float scale,
                 FodderColor color)
{
    for (; *message; ++message, x += 6 * scale) {
        unsigned char c = (unsigned char)toupper((unsigned char)*message);
        if (c >= 127 || c < 32)
            c = '?';
        for (unsigned row = 0; row < 7; ++row) {
            for (unsigned column = 0; column < 5; ++column) {
                if (r->font[c][row] & (1u << (4 - column)))
                    rectangle(r, x + scale * (float)column, y + scale * (float)row, scale, scale, color);
            }
        }
    }
}

static void centered(FodderRenderer *r, const char *message, float y, float s, FodderColor color)
{
    text(r, message, 480 - (float)strlen(message) * 3 * s, y, s, color);
}

static void sky(FodderRenderer *r, float fog)
{
    const FodderColor horizon = {0.63f, 0.54f, 0.42f, 1};
    for (unsigned band = 0; band < 18; ++band) {
        float fall = (float)band / 18;
        FodderColor color = {0.19f + fall * 0.43f, 0.27f + fall * 0.27f,
                             0.31f + fall * 0.11f, 1};
        if (fog < 60) {
            color.r = color.r * 0.80f + 0.09f;
            color.b = color.b * 0.85f + 0.11f;
        }
        rectangle(r, 0, band * 22.0f, 960, 22, color);
    }
    rectangle(r, 0, 396, 960, 144, horizon);
    overlay_draw(r);
}

static void draw_world(FodderRenderer *r, const FodderScene *scene, const FodderSession *session,
                       FodderMatrix camera, FodderVec eye, float yaw)
{
    const FodderBattle *battle = &session->battle;
    FodderGL *gl = &r->gl;
    FodderMatrix identity = fodder_identity();
    Instance neutral = {0, 0, 0, 0, {1, 1, 1, 1}};
    gl->UniformMatrix4fv(r->camera, 1, GL_FALSE, camera.data);
    gl->Uniform3f(r->eye, eye.x, eye.y, eye.z);
    gl->Uniform1f(r->fog, battle->variant.fog);
    gl->Uniform3f(r->haze, 0.63f, 0.54f, 0.42f);
    draw(r, &scene->world, 0, 0, identity, &neutral, 1);
    draw(r, &scene->pennants, 2, scene->banner, identity, &neutral, 1);
    unsigned close = 0, far = 0;
    float sx = sinf(yaw), cz = cosf(yaw);
    for (unsigned i = 1; i < battle->count; ++i) {
        const FodderSoldier *unit = &battle->soldiers[i];
        float dx = unit->x - eye.x, dz = unit->z - eye.z;
        float length = dx * dx + dz * dz;
        if (length > 18500 || sx * dx - cz * dz < -8)
            continue;
        float y = fodder_ground(battle->arena, unit->x, unit->z);
        float bob = 0.047f * sinf((float)battle->tick * 0.14f + (float)i * 2.513f);
        if (fabsf(unit->vx) + fabsf(unit->vz) > 0.1f)
            y += bob;
        FodderColor dye = colors[unit->side];
        if (unit->windup > 0) {
            dye.r = fminf(1, dye.r + 0.37f);
            dye.g = fminf(1, dye.g + 0.10f);
        }
        if (unit->health <= 0) {
            if (length > 28 * 28)
                continue;
            dye.a = -1;
        }
        Instance instance = {unit->x, y, unit->z, unit->yaw, dye};
        if (length < 26 * 26 || unit->health <= 0) {
            r->close[close++] = instance;
        } else {
            instance.yaw = yaw;
            r->far[far++] = instance;
        }
    }
    draw(r, &scene->levy, 0, 0, identity, r->close, close);
    draw(r, &r->billboard, 1, scene->sprite, identity, r->far, far);
}

static void equipment(FodderRenderer *r, const FodderScene *scene, const FodderSession *session,
                      FodderMatrix camera, FodderMatrix pose)
{
    const FodderBattle *battle = &session->battle;
    float wind = battle->attack_time > 0 ? sinf(3.1415927f * battle->attack_time / 0.56f) : 0;
    float lift = battle->guarding ? 0.35f : 0.0f;
    float walk = sinf((float)battle->tick * 0.19f) * 0.017f *
                 fminf(1, hypotf(battle->soldiers[0].vx, battle->soldiers[0].vz));
    FodderMatrix sword = fodder_transform((FodderVec){0.45f - wind * 0.47f, -0.38f + walk, -0.76f},
                                          0.10f - wind * 0.85f, -0.37f + wind * 1.05f,
                                          -0.28f + wind * 1.82f, 0.66f);
    FodderMatrix shield = fodder_transform((FodderVec){-0.52f + lift * 0.46f,
                                           -0.51f + lift + walk, -0.74f + lift * 0.40f},
                                           -0.22f, 0.23f, 0.29f - lift, 0.71f);
    r->gl.UniformMatrix4fv(r->camera, 1, GL_FALSE, camera.data);
    r->gl.Uniform1f(r->fog, 0);
    const Instance neutral = {0, 0, 0, 0, {0.23f, 0.72f, 0.55f, 1}};
    draw(r, &scene->sword, 0, 0, fodder_multiply(pose, sword), &neutral, 1);
    draw(r, &scene->shield, 0, 0, fodder_multiply(pose, shield), &neutral, 1);
}

static void fight_hud(FodderRenderer *r, const FodderSession *session, bool xr)
{
    const FodderBattle *battle = &session->battle;
    char line[96];
    float x = xr ? 280 : 24, lower = xr ? 374 : 469;
    float scale = xr ? 1.45f : 2.0f;
    float width = xr ? 112 : 183;
    unsigned seconds = fodder_time_left(battle);
    rectangle(r, x - 8, lower - 12, width + 16, 59, (FodderColor){0.05f, 0.07f, 0.07f, 0.87f});
    text(r, "HEALTH", x, lower, scale, ivory);
    rectangle(r, x, lower + 18, width, 4, (FodderColor){0.26f, 0.19f, 0.14f, 1});
    rectangle(r, x, lower + 18, width * battle->soldiers[0].health / 100, 4, colors[0]);
    text(r, "STAMINA", x, lower + 28, scale * 0.78f, gray);
    rectangle(r, x + (xr ? 54 : 76), lower + 29, (xr ? 58 : 107) * battle->stamina, 5, gold);
    snprintf(line, sizeof(line), "%02u:%02u", seconds / 60, seconds % 60);
    centered(r, line, xr ? 135 : 20, scale * 1.23f, ivory);
    snprintf(line, sizeof(line), "%03u ALLIES  /  %03u ENEMIES", battle->alive[0], battle->alive[1]);
    centered(r, line, xr ? 152 : 43, scale * 0.8f, gray);
    snprintf(line, sizeof(line), "KILLS %03u", battle->kills);
    text(r, line, xr ? 610 : 820, lower + 10, scale, gold);
    if (!xr) {
        rectangle(r, 478, 269, 4, 2, ivory);
        rectangle(r, 479, 268, 2, 4, ivory);
    }
    if (battle->hurt > 0.05f)
        rectangle(r, 0, 0, 960, 540, (FodderColor){0.57f, 0.14f, 0.09f, battle->hurt * 0.18f});
}

static void menu_hud(FodderRenderer *r, const FodderSession *s, bool xr)
{
    float margin = xr ? 210 : 144, top = xr ? 134 : 90;
    float scale = xr ? 1.5f : 2.0f;
    rectangle(r, margin, top, 960 - 2 * margin, xr ? 266 : 350,
              (FodderColor){0.04f, 0.063f, 0.063f, 0.93f});
    rectangle(r, margin, top, 960 - 2 * margin, 2, gold);
    if (s->saving || s->save_error) {
        centered(r, s->saving ? "SAVING PROGRESSION" : "SAVE FAILED", top + 75, scale * 2, gold);
        if (s->save_error)
            centered(r, xr ? "TRIGGER TO RETRY" : "ENTER TO RETRY", top + 141, scale, gray);
        return;
    }
    switch (s->screen) {
    case FODDER_CAMP:
        centered(r, "FODDER", top + 40, scale * 4, ivory);
        centered(r, s->battle.variant.name, top + 112, scale * 1.50f, gold);
        centered(r, "1 SOLDIER / 1000 ON THE FIELD", top + 150, scale, gray);
        centered(r, xr ? "TRIGGER TO DEPLOY" : "ENTER TO DEPLOY", top + 191, scale * 1.17f, ivory);
        centered(r, xr ? "STICK MOVE / TRIGGER STRIKE / GRIP BLOCK" : "WASD MOVE / MOUSE LOOK / LEFT STRIKE / RIGHT BLOCK",
                 top + 236, scale * 0.75f, gray);
        break;
    case FODDER_PAUSED:
        centered(r, "BATTLE PAUSED", top + 76, scale * 2, ivory);
        centered(r, xr ? "TRIGGER TO RESUME" : "PRESS ENTER TO RESUME", top + 143, scale, gold);
        break;
    case FODDER_REWARD:
        centered(r, "YOU SURVIVED", top + 38, scale * 2, ivory);
        centered(r, "CHOOSE A PERMANENT UPGRADE", top + 81, scale * 0.92f, gold);
        for (unsigned i = 0; i < FODDER_UPGRADES; ++i) {
            char choice[80];
            snprintf(choice, sizeof(choice), "%u  %s  +1", i + 1, s->pack->upgrades[i].name);
            centered(r, choice, top + 115 + 24 * (float)i, scale, s->selection == i ? gold : ivory);
        }
        centered(r, xr ? "STICK SELECT / TRIGGER CLAIM" : "1-3 CHOOSE / ENTER CONFIRM", top + 216, scale * 0.81f, gray);
        break;
    case FODDER_RESULT: {
        centered(r, "FALLEN IN BATTLE", top + 52, scale * 1.9f, ivory);
        char result[48];
        snprintf(result, sizeof(result), "%u ENEMY KILLS", s->profile.last_kills);
        centered(r, result, top + 107, scale * 1.22f, gold);
        centered(r, "PERMANENT UPGRADES KEPT", top + 143, scale, gray);
        centered(r, xr ? "TRIGGER TO RETRY" : "PRESS ENTER TO REDEPLOY", top + 192, scale, ivory);
        break;
    }
    case FODDER_FIGHT:
        break;
    }
}

static void viewport_begin(FodderRenderer *r, const FodderSession *s,
                           int x, int y, int w, int h, bool xr)
{
    FodderGL *gl = &r->gl;
    gl->Viewport(x, y, w, h);
    if (xr) {
        gl->Enable(GL_SCISSOR_TEST);
        gl->Scissor(x, y, w, h);
    }
    gl->ClearColor(0.40f, 0.39f, 0.36f, 1);
    gl->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl->UseProgram(r->program);
    gl->Disable(GL_BLEND);
    gl->Disable(GL_DEPTH_TEST);
    sky(r, s->battle.variant.fog);
    gl->Enable(GL_DEPTH_TEST);
}

static void viewport_end(FodderRenderer *r, const FodderSession *session, bool web_menu, bool xr)
{
    FodderGL *gl = &r->gl;
    gl->Disable(GL_DEPTH_TEST);
    gl->Enable(GL_BLEND);
    if (session->screen == FODDER_FIGHT && !session->saving && !session->save_error)
        fight_hud(r, session, xr);
    else if (!web_menu)
        menu_hud(r, session, xr);
    overlay_draw(r);
    gl->Disable(GL_BLEND);
    gl->Disable(GL_SCISSOR_TEST);
    gl->BindVertexArray(0);
}

void fodder_screen_draw(FodderRenderer *r, const FodderScene *scene,
                         const FodderSession *s, int w, int h, bool web_menu)
{
    if (w <= 0 || h <= 0)
        return;
    const FodderSoldier *player = s->battle.soldiers;
    bool camp = s->screen == FODDER_CAMP;
    float yaw = player->yaw + (camp ? (web_menu ? 0.18f : -0.11f) : 0);
    float pitch = camp ? -0.12f : s->battle.pitch;
    FodderVec eye = {player->x + (camp ? -5.0f : 0), 1.68f, player->z + (camp ? 8.0f : 0)};
    eye.y += fodder_ground(s->battle.arena, eye.x, eye.z) + (camp ? 1.78f : 0);
    float aspect = (float)w / (float)h;
    FodderMatrix view = fodder_view(eye, yaw, pitch);
    FodderMatrix projection = fodder_perspective(1.20f, aspect, 0.045f, 260.0f);
    viewport_begin(r, s, 0, 0, w, h, false);
    draw_world(r, scene, s, fodder_multiply(projection, view), eye, yaw);
    if (s->screen == FODDER_FIGHT) {
        r->gl.Clear(GL_DEPTH_BUFFER_BIT);
        equipment(r, scene, s, projection, fodder_identity());
    }
    viewport_end(r, s, web_menu, false);
}

void fodder_eye_draw(FodderRenderer *r, const FodderScene *scene,
                      const FodderSession *s, const float matrices[36], int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    FodderMatrix camera, hand;
    memcpy(camera.data, matrices, sizeof(camera.data));
    memcpy(hand.data, matrices + 16, sizeof(hand.data));
    FodderVec eye = {matrices[32], matrices[33], matrices[34]};
    viewport_begin(r, s, x, y, w, h, true);
    draw_world(r, scene, s, camera, eye, matrices[35]);
    if (s->screen == FODDER_FIGHT) {
        r->gl.Clear(GL_DEPTH_BUFFER_BIT);
        equipment(r, scene, s, hand, fodder_identity());
    }
    viewport_end(r, s, false, true);
}

bool fodder_renderer_check(FodderRenderer *r)
{
    return r->gl.GetError() == GL_NO_ERROR;
}
