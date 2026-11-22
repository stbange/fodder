#include "geometry.h"
#include "pack.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FodderMatrix fodder_identity(void)
{
    return (FodderMatrix){.data = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
}

FodderMatrix fodder_multiply(FodderMatrix left, FodderMatrix right)
{
    FodderMatrix output = {0};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            for (int i = 0; i < 4; ++i)
                output.data[column * 4 + row] += left.data[i * 4 + row] * right.data[column * 4 + i];
        }
    }
    return output;
}

FodderMatrix fodder_perspective(float fov, float aspect, float near_z, float far_z)
{
    float scale = 1.0f / tanf(fov * 0.5f);
    FodderMatrix m = {0};
    m.data[0] = scale / aspect;
    m.data[5] = scale;
    m.data[10] = (far_z + near_z) / (near_z - far_z);
    m.data[11] = -1.0f;
    m.data[14] = 2.0f * near_z * far_z / (near_z - far_z);
    return m;
}

static float dot(FodderVec a, FodderVec b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

FodderMatrix fodder_view(FodderVec eye, float yaw, float pitch)
{
    float sy = sinf(yaw), cy = cosf(yaw), sp = sinf(pitch), cp = cosf(pitch);
    FodderVec right = {cy, 0, sy};
    FodderVec up = {-sy * sp, cp, cy * sp};
    FodderVec back = {-sy * cp, -sp, cy * cp};
    return (FodderMatrix){.data = {
        right.x, up.x, back.x, 0,
        right.y, up.y, back.y, 0,
        right.z, up.z, back.z, 0,
        -dot(right, eye), -dot(up, eye), -dot(back, eye), 1
    }};
}

FodderMatrix fodder_transform(FodderVec position, float yaw, float pitch, float roll, float scale)
{
    float sy = sinf(yaw), cy = cosf(yaw), sp = sinf(pitch), cp = cosf(pitch);
    float sr = sinf(roll), cr = cosf(roll);
    FodderVec right = {cy, 0, sy};
    FodderVec up = {-sy * sp, cp, cy * sp};
    FodderVec back = {-sy * cp, -sp, cy * cp};
    return (FodderMatrix){.data = {
        (right.x * cr + up.x * sr) * scale, (right.y * cr + up.y * sr) * scale,
        (right.z * cr + up.z * sr) * scale, 0,
        (up.x * cr - right.x * sr) * scale, (up.y * cr - right.y * sr) * scale,
        (up.z * cr - right.z * sr) * scale, 0,
        back.x * scale, back.y * scale, back.z * scale, 0,
        position.x, position.y, position.z, 1
    }};
}

static bool append(FodderMesh *mesh, const FodderVertex *vertices, unsigned count)
{
    if (mesh->count + count > mesh->capacity) {
        unsigned capacity = 2 * mesh->capacity + 64;
        FodderVertex *buffer = realloc(mesh->vertices, capacity * sizeof(*buffer));
        if (!buffer)
            return false;
        mesh->vertices = buffer;
        mesh->capacity = capacity;
    }
    memcpy(mesh->vertices + mesh->count, vertices, count * sizeof(*vertices));
    mesh->count += count;
    return true;
}

bool fodder_quad(FodderMesh *mesh, FodderVec a, FodderVec b, FodderVec c, FodderVec d,
                 FodderColor color)
{
    const FodderVertex vertices[] = {
        {a, 0, 0, color}, {b, 1, 0, color}, {c, 1, 1, color},
        {a, 0, 0, color}, {c, 1, 1, color}, {d, 0, 1, color}
    };
    return append(mesh, vertices, 6);
}

void fodder_mesh_free(FodderMesh *mesh)
{
    free(mesh->vertices);
    *mesh = (FodderMesh){0};
}

bool fodder_mesh_read(FodderMesh *mesh, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    unsigned size;
    char magic[20];
    bool valid = fscanf(file, "%19s %u", magic, &size) == 2 &&
                 strcmp(magic, "FODDERM1") == 0 && size > 0 && size <= 18000 && size % 3 == 0;
    for (unsigned i = 0; valid && i < size; ++i) {
        FodderVertex vertex = {0};
        FodderVec *p = &vertex.position;
        FodderColor *c = &vertex.color;
        valid = fscanf(file, "%f %f %f %f %f %f %f", &p->x, &p->y, &p->z, &c->r, &c->g, &c->b, &c->a) == 7 &&
                fabsf(p->x) <= 100 && fabsf(p->y) <= 100 && fabsf(p->z) <= 100 &&
                c->r >= 0 && c->r <= 1 && c->g >= 0 && c->g <= 1 &&
                c->b >= 0 && c->b <= 1 && c->a >= 0 && c->a <= 1;
        if (valid)
            valid = append(mesh, &vertex, 1);
    }
    fclose(file);
    if (!valid)
        fodder_mesh_free(mesh);
    return valid;
}

bool fodder_image_read(FodderImage *image, const char *path)
{
    size_t length;
    unsigned char *file = (unsigned char *)fodder_read(path, 4 * 512 * 512 + 18, &length);
    if (!file)
        return false;
    if (length < 18 || file[0] != 0 || file[1] != 0 || file[2] != 2 ||
        file[16] != 32 || file[17] != 8) {
        free(file);
        return false;
    }
    unsigned width = file[12] + 256u * file[13];
    unsigned height = file[14] + 256u * file[15];
    size_t count = 4u * (size_t)width * height;
    if (!width || !height || width > 512 || height > 512 || length != 18 + count) {
        free(file);
        return false;
    }
    for (size_t i = 0; i < count; i += 4) {
        unsigned char blue = file[18 + i];
        file[18 + i] = file[20 + i];
        file[20 + i] = blue;
    }
    memmove(file, file + 18, count);
    *image = (FodderImage){width, height, file};
    return true;
}

void fodder_image_free(FodderImage *image)
{
    free(image->pixels);
    *image = (FodderImage){0};
}

static bool box(FodderMesh *mesh, FodderVec a, FodderVec b, FodderColor color)
{
    FodderVec q[] = {
        {a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, a.y, b.z}, {a.x, a.y, b.z},
        {a.x, b.y, a.z}, {b.x, b.y, a.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}
    };
    FodderColor side = {color.r * 0.70f, color.g * 0.73f, color.b * 0.79f, color.a};
    FodderColor dark = {color.r * 0.50f, color.g * 0.56f, color.b * 0.63f, color.a};
    return fodder_quad(mesh, q[4], q[5], q[6], q[7], color) &&
           fodder_quad(mesh, q[0], q[1], q[5], q[4], side) &&
           fodder_quad(mesh, q[1], q[2], q[6], q[5], dark) &&
           fodder_quad(mesh, q[2], q[3], q[7], q[6], side) &&
           fodder_quad(mesh, q[3], q[0], q[4], q[7], dark);
}

static float landscape_height(const FodderArena *arena, float x, float z)
{
    float outside = fmaxf(fabsf(x) - arena->half_width, fabsf(z) - arena->half_length);
    float mountain = fminf(fmaxf(outside, 0) * 0.28f, 25.0f);
    return fodder_ground(arena, x, z) + mountain * (1 + 0.68f * sinf(x * 0.052f + z * 0.025f));
}

static bool terrain(FodderMesh *mesh, const FodderArena *arena)
{
    for (int z = -128; z < 128; z += 4) {
        for (int x = -128; x < 128; x += 4) {
            float wave = sinf(x * 0.65f + z * 0.24f) * 0.029f;
            float X = (float)x, Z = (float)z;
            float bank = fabsf(Z + 2);
            FodderColor color = {0.30f + wave, 0.31f + wave, 0.22f + wave, 0};
            if (bank < arena->channel + 1.0f) {
                color = fabsf(X + 2) < arena->ford ?
                        (FodderColor){0.42f + wave, 0.38f + wave, 0.29f + wave, 0} :
                        (FodderColor){0.13f + wave, 0.33f + wave, 0.34f + wave, 0};
            } else if (fabsf(X) > arena->half_width + 20 || fabsf(Z) > arena->half_length + 16) {
                color = (FodderColor){0.27f + wave, 0.30f + wave, 0.32f + wave, 0};
            }
            FodderVec a = {X, landscape_height(arena, X, Z), Z};
            FodderVec b = {X + 4, landscape_height(arena, X + 4, Z), Z};
            FodderVec c = {X + 4, landscape_height(arena, X + 4, Z + 4), Z + 4};
            FodderVec d = {X, landscape_height(arena, X, Z + 4), Z + 4};
            if (!fodder_quad(mesh, a, b, c, d, color))
                return false;
        }
    }
    return true;
}

bool fodder_landscape(FodderMesh *mesh, const FodderArena *arena)
{
    if (!terrain(mesh, arena))
        return false;
    for (unsigned i = 0; i < arena->obstacle_count; ++i) {
        const FodderObstacle *o = &arena->obstacles[i];
        float y = fodder_ground(arena, o->x, o->z);
        FodderVec a = {o->x - o->half_x, y - 0.2f, o->z - o->half_z};
        FodderVec b = {o->x + o->half_x, y + o->height, o->z + o->half_z};
        if (!box(mesh, a, b, (FodderColor){0.47f, 0.39f, 0.33f, 0}))
            return false;
        for (int stone = 0; stone < 4; ++stone) {
            float x = a.x + (b.x - a.x) * ((float)stone + 0.15f) / 4.0f;
            float w = (b.x - a.x) / 7.0f;
            if (!box(mesh, (FodderVec){x, b.y - 0.04f, a.z + 0.04f},
                     (FodderVec){x + w, b.y + 0.24f + 0.04f * (float)stone, b.z - 0.04f},
                     (FodderColor){0.56f, 0.48f, 0.38f, 0}))
                return false;
        }
    }
    for (int i = 0; i < 50; ++i) {
        float x = (float)(((i * 37) % 112) - 56);
        float z = (float)(((i * 29) % 128) - 64);
        if (fabsf(z) < arena->channel + 1 || fabsf(x) < 3)
            continue;
        float y = fodder_ground(arena, x, z);
        FodderColor rock = {0.33f, 0.35f, 0.30f, 0};
        if (!box(mesh, (FodderVec){x, y, z},
                 (FodderVec){x + 0.7f, y + 0.11f, z + 0.4f}, rock))
            return false;
    }
    return true;
}

bool fodder_banners(FodderMesh *mesh, const FodderArena *arena)
{
    for (int side = -1; side <= 1; side += 2) {
        float x = 7.6f * (float)side, z = 8.7f * (float)side;
        float base = fodder_ground(arena, x, z);
        FodderColor tint = side > 0 ? (FodderColor){0.38f, 0.82f, 0.69f, 1} :
                                   (FodderColor){1.00f, 0.47f, 0.30f, 1};
        if (!fodder_quad(mesh, (FodderVec){x, base, z}, (FodderVec){x + 0.12f, base, z},
                         (FodderVec){x + 0.12f, base + 5, z}, (FodderVec){x, base + 5, z}, tint) ||
            !fodder_quad(mesh, (FodderVec){x, base + 2.8f, z + 0.02f},
                         (FodderVec){x + 1.6f, base + 2.8f, z + 0.22f},
                         (FodderVec){x + 1.6f, base + 4.95f, z + 0.22f},
                         (FodderVec){x, base + 4.95f, z + 0.02f}, tint))
            return false;
    }
    return true;
}

bool fodder_font_read(unsigned char (*font)[7], const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    bool valid = true;
    unsigned code;
    while (fscanf(file, "%u", &code) == 1) {
        if (code < 32 || code > 126) {
            valid = false;
            break;
        }
        for (int row = 0; row < 7; ++row) {
            unsigned value;
            if (fscanf(file, "%x", &value) != 1 || value > 31) {
                valid = false;
                break;
            }
            font[code][row] = (unsigned char)value;
        }
        if (!valid)
            break;
    }
    bool ended = feof(file) != 0;
    fclose(file);
    return valid && ended && font['A'][0] && font['0'][0];
}
