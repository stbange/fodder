#ifndef FODDER_RENDER_H
#define FODDER_RENDER_H

#include "geometry.h"
#include "session.h"

typedef struct FodderRenderer FodderRenderer;
typedef struct FodderScene FodderScene;

FodderRenderer *fodder_renderer_open(const char *assets, char error[FODDER_ERROR]);
void fodder_renderer_close(FodderRenderer *renderer);
FodderScene *fodder_scene_load(FodderRenderer *renderer, const FodderPack *pack,
                               char error[FODDER_ERROR]);
void fodder_scene_free(FodderRenderer *renderer, FodderScene *scene);
void fodder_screen_draw(FodderRenderer *renderer, const FodderScene *scene,
                         const FodderSession *session, int w, int h, bool web_menu);
void fodder_eye_draw(FodderRenderer *renderer, const FodderScene *scene,
                      const FodderSession *session, const float matrices[36],
                      int x, int y, int w, int h);
bool fodder_renderer_check(FodderRenderer *renderer);

#endif
