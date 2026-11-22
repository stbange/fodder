#ifndef FODDER_GEOMETRY_H
#define FODDER_GEOMETRY_H

#include "battle.h"
#include <stddef.h>

typedef struct { float x, y, z; } FodderVec;
typedef struct { float r, g, b, a; } FodderColor;
typedef struct { float data[16]; } FodderMatrix;

typedef struct {
    FodderVec position;
    float u, v;
    FodderColor color;
} FodderVertex;

typedef struct {
    FodderVertex *vertices;
    unsigned count, capacity;
} FodderMesh;

typedef struct {
    unsigned width, height;
    unsigned char *pixels;
} FodderImage;

FodderMatrix fodder_identity(void);
FodderMatrix fodder_multiply(FodderMatrix left, FodderMatrix right);
FodderMatrix fodder_perspective(float fov, float aspect, float near_z, float far_z);
FodderMatrix fodder_view(FodderVec eye, float yaw, float pitch);
FodderMatrix fodder_transform(FodderVec position, float yaw, float pitch, float roll, float scale);

bool fodder_mesh_read(FodderMesh *mesh, const char *path);
void fodder_mesh_free(FodderMesh *mesh);
bool fodder_quad(FodderMesh *mesh, FodderVec a, FodderVec b, FodderVec c, FodderVec d,
                 FodderColor color);
bool fodder_image_read(FodderImage *image, const char *path);
void fodder_image_free(FodderImage *image);
bool fodder_landscape(FodderMesh *mesh, const FodderArena *arena);
bool fodder_banners(FodderMesh *mesh, const FodderArena *arena);
bool fodder_font_read(unsigned char (*font)[7], const char *path);

#endif
