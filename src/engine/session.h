#ifndef FODDER_SESSION_H
#define FODDER_SESSION_H

#include "pack.h"
#include "profile.h"

typedef enum {
    FODDER_CAMP, FODDER_FIGHT, FODDER_PAUSED, FODDER_REWARD, FODDER_RESULT
} FodderScreen;

typedef enum {
    FODDER_CONFIRM, FODDER_BACK, FODDER_PREVIOUS, FODDER_NEXT,
    FODDER_FIRST_UPGRADE, FODDER_SECOND_UPGRADE, FODDER_THIRD_UPGRADE
} FodderAction;

typedef enum { FODDER_WRITE_FAILED, FODDER_WRITE_COMPLETE, FODDER_WRITE_PENDING } FodderWrite;
typedef FodderWrite (*FodderSave)(void *data, const FodderProfile *previous, const FodderProfile *next);

typedef struct {
    const FodderPack *pack;
    FodderBattle battle;
    FodderProfile profile, pending_profile;
    FodderSave save;
    void *storage;
    FodderScreen screen, after_save;
    unsigned selection;
    bool saving, save_error, attack_down, queued_attack;
    double time_buffer;
    float queued_x, queued_y;
} FodderSession;

void fodder_session_open(FodderSession *session, const FodderPack *pack, FodderProfile profile,
                          FodderSave save, void *storage);
void fodder_session_action(FodderSession *session, FodderAction action);
void fodder_session_frame(FodderSession *session, double elapsed, FodderInput input);
void fodder_session_saved(FodderSession *session, bool success);
void fodder_session_clear_input(FodderSession *session);

#endif
