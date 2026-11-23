#include "profile_store.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HANDLE FileLock;
static FileLock lock_profile(const char *path)
{
    wchar_t *wide = (wchar_t *)SDL_iconv_string("UTF-16LE", "UTF-8", path, strlen(path) + 1);
    if (!wide)
        return INVALID_HANDLE_VALUE;
    HANDLE handle = CreateFileW(wide, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    SDL_free(wide);
    return handle;
}
#define LOCK_INVALID INVALID_HANDLE_VALUE
#define UNLOCK(handle) CloseHandle(handle)
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
typedef int FileLock;
static FileLock lock_profile(const char *path)
{
    int handle = open(path, O_CREAT | O_WRONLY, 0600);
    if (handle >= 0 && flock(handle, LOCK_EX | LOCK_NB) != 0) {
        close(handle);
        handle = -1;
    }
    return handle;
}
#define LOCK_INVALID (-1)
#define UNLOCK(handle) close(handle)
#endif

static bool profile_path(char path[FODDER_PATH], const char *id, const char *suffix)
{
    char *pref = SDL_GetPrefPath("Fodder", "Fodder");
    if (!pref)
        return false;
    char filename[80];
    snprintf(filename, sizeof(filename), "%s.%s", id, suffix);
    bool joined = fodder_path_join(path, FODDER_PATH, pref, filename);
    SDL_free(pref);
    return joined;
}

static bool read_path(const char *path, unsigned variants, FodderProfile *profile)
{
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info))
        return true;
    if (info.type != SDL_PATHTYPE_FILE)
        return false;
    size_t length;
    char *text = fodder_read(path, FODDER_PROFILE_TEXT - 1, &length);
    bool valid = text && fodder_profile_decode(profile, text, variants);
    free(text);
    return valid;
}

bool fodder_profile_read_file(const char *id, unsigned variants, FodderProfile *profile)
{
    char path[FODDER_PATH];
    return profile_path(path, id, "profile") && read_path(path, variants, profile);
}

bool fodder_profile_write_file(const char *id, unsigned variants, unsigned revision, const char *text)
{
    char path[FODDER_PATH], temporary[FODDER_PATH], lock_path[FODDER_PATH];
    if (!profile_path(path, id, "profile") || !profile_path(temporary, id, "tmp") ||
        !profile_path(lock_path, id, "lock"))
        return false;
    FileLock lock = lock_profile(lock_path);
    if (lock == LOCK_INVALID)
        return false;
    FodderProfile current = {0};
    bool valid = read_path(path, variants, &current) && current.revision == revision &&
                 SDL_SaveFile(temporary, text, strlen(text));
    if (valid && !SDL_RenamePath(temporary, path)) {
        SDL_RemovePath(temporary);
        valid = false;
    }
    UNLOCK(lock);
    return valid;
}
