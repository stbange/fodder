#ifndef FODDER_PACK_H
#define FODDER_PACK_H

#include "battle.h"
#include <stddef.h>

enum { FODDER_PATH = 768, FODDER_FILE = 160, FODDER_PACK_ID = 33, FODDER_ERROR = 256 };

typedef enum {
    FODDER_SCRIPT, FODDER_SOLDIER_MESH, FODDER_SWORD_MESH, FODDER_SHIELD_MESH,
    FODDER_SPRITE, FODDER_BANNER, FODDER_STRIKE_AUDIO, FODDER_HORN_AUDIO,
    FODDER_CROWD_AUDIO, FODDER_PACK_FILES
} FodderPackFile;

typedef struct {
    char name[FODDER_LABEL];
    char description[96];
} FodderUpgrade;

typedef struct {
    char id[FODDER_PACK_ID];
    char title[FODDER_LABEL];
    char root[FODDER_PATH];
    char files[FODDER_PACK_FILES][FODDER_FILE];
    FodderArena arena;
    FodderVariant variants[FODDER_VARIANTS];
    FodderUpgrade upgrades[FODDER_UPGRADES];
    unsigned variant_count;
    void *script;
    int rules_reference;
} FodderPack;

bool fodder_valid_id(const char *id);
bool fodder_valid_path(const char *path);
bool fodder_path_join(char *destination, size_t length, const char *root, const char *file);
char *fodder_read(const char *path, size_t limit, size_t *length);
bool fodder_pack_open(FodderPack *pack, const char *root, char error[FODDER_ERROR]);
void fodder_pack_close(FodderPack *pack);
FodderEffects fodder_pack_effects(const FodderPack *pack, const unsigned ranks[FODDER_UPGRADES]);
bool fodder_pack_file(const FodderPack *pack, FodderPackFile file, char path[FODDER_PATH]);

#endif
