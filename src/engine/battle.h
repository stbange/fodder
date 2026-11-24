#ifndef FODDER_BATTLE_H
#define FODDER_BATTLE_H

#include <stdbool.h>
#include <stdint.h>

enum {
    FODDER_ARMY_SIZE = 500,
    FODDER_SOLDIERS = 2 * FODDER_ARMY_SIZE,
    FODDER_TICKS_PER_SECOND = 60,
    FODDER_VARIANTS = 8,
    FODDER_OBSTACLES = 32,
    FODDER_GRID_SIDE = 48,
    FODDER_UPGRADES = 3,
    FODDER_DEEDS_FOR_BONUS = 2,
    FODDER_MAX_FLANKERS = 24,
    FODDER_LABEL = 48
};

typedef struct {
    float x, z, half_x, half_z, height;
} FodderObstacle;

typedef struct {
    char name[FODDER_LABEL];
    float half_width, half_length, channel, ford, dip;
    unsigned obstacle_count;
    FodderObstacle obstacles[FODDER_OBSTACLES];
} FodderArena;

typedef struct {
    char name[FODDER_LABEL];
    char hint[96];
    unsigned seconds, flankers;
    float rout_fraction;
    float enemy_speed, ally_speed, enemy_damage, ally_damage;
    float player_damage, fog;
    int ally_lane, enemy_lane;
} FodderVariant;

typedef struct {
    float recovery, block_cost, dressing;
} FodderEffects;

typedef struct {
    float move, strafe;
    float look_x, look_y;
    bool attack, block, attack_pressed;
} FodderInput;

typedef enum {
    FODDER_STRIKE_SLASH,
    FODDER_STRIKE_OVERHEAD
} FodderStrike;

typedef struct {
    float x, z, yaw, vx, vz;
    float health, cooldown, windup, windup_duration, recovery, stagger;
    unsigned provoked_until;
    uint16_t squad;
    int16_t target;
    uint8_t side, flank_stage, strike;
    bool guarding;
} FodderSoldier;

typedef enum {
    FODDER_ACTIVE,
    FODDER_SURVIVED,
    FODDER_FALLEN
} FodderOutcome;

typedef enum {
    FODDER_ONGOING,
    FODDER_ROUT,
    FODDER_HORN,
    FODDER_DEATH
} FodderEnd;

typedef enum {
    FODDER_COMBAT_NONE,
    FODDER_COMBAT_HIT,
    FODDER_COMBAT_MISS,
    FODDER_COMBAT_BLOCKED,
    FODDER_COMBAT_OBSTRUCTED,
    FODDER_COMBAT_COUNTER,
    FODDER_COMBAT_PARRY,
    FODDER_COMBAT_GUARDED,
    FODDER_COMBAT_GUARD_BROKEN
} FodderCombatFeedback;

typedef struct {
    const FodderArena *arena;
    FodderVariant variant;
    FodderEffects effects;
    FodderSoldier soldiers[FODDER_SOLDIERS];
    int16_t grid[FODDER_GRID_SIDE * FODDER_GRID_SIDE];
    int16_t next[FODDER_SOLDIERS], previous[FODDER_SOLDIERS], cells[FODDER_SOLDIERS];
    float incoming[FODDER_SOLDIERS];
    uint8_t strikes_at[FODDER_SOLDIERS];
    unsigned count, starting_per_side, alive[2], tick, kills, deeds, feedback_tick;
    uint32_t rng;
    float stamina, pitch, hurt;
    float guard_window, guard_rearm, counter_time, feedback_time;
    int counter_target, counter_swing, attacker, flanker;
    bool attack_held, block_held, dressed, sword_recoil;
    FodderCombatFeedback feedback;
    int winner;
    FodderOutcome outcome;
    FodderEnd end;
} FodderBattle;

void fodder_battle_start(FodderBattle *battle, const FodderArena *arena,
                         const FodderVariant *variant, FodderEffects effects,
                         uint32_t seed, unsigned per_side);
void fodder_battle_tick(FodderBattle *battle, FodderInput input);
float fodder_ground(const FodderArena *arena, float x, float z);
float fodder_clamp(float value, float low, float high);
float fodder_wrap(float angle);
float fodder_bearing(const FodderSoldier *observer, const FodderSoldier *other);
float fodder_strike_phase(const FodderSoldier *soldier);
int fodder_counter_opponent(const FodderBattle *battle);
unsigned fodder_time_left(const FodderBattle *battle);

#endif
