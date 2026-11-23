#ifndef FODDER_PROFILE_STORE_H
#define FODDER_PROFILE_STORE_H

#include "pack.h"
#include "profile.h"

bool fodder_profile_read_file(const char *id, unsigned variants, FodderProfile *profile);
bool fodder_profile_write_file(const char *id, unsigned variants, unsigned revision, const char *text);

#endif
