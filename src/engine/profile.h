#ifndef FODDER_PROFILE_H
#define FODDER_PROFILE_H

#include "battle.h"
#include <stddef.h>

enum { FODDER_PROFILE_TEXT = 256 };

typedef struct {
    unsigned revision, attempts, variant;
    unsigned rank[FODDER_UPGRADES];
    unsigned best_kills, streak, best_streak, last_kills;
    bool pending;
} FodderProfile;

bool fodder_profile_decode(FodderProfile *profile, const char *text, unsigned variants);
bool fodder_profile_encode(const FodderProfile *profile, char *text, size_t length);
bool fodder_profile_result(FodderProfile *next, const FodderProfile *profile,
                           FodderOutcome outcome, unsigned kills);
bool fodder_profile_claim(FodderProfile *next, const FodderProfile *profile,
                          unsigned upgrade, unsigned variants);
uint32_t fodder_profile_seed(const FodderProfile *profile);

#endif
