#ifndef FODDER_SOUND_H
#define FODDER_SOUND_H

#include "session.h"

typedef struct FodderSound FodderSound;
FodderSound *fodder_sound_load(const FodderPack *pack, char error[FODDER_ERROR]);
void fodder_sound_free(FodderSound *sound);
void fodder_sound_enable(FodderSound *sound);
void fodder_sound_frame(FodderSound *sound, const FodderSession *session);

#endif
