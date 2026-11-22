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
    unsigned seconds;
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
    bool attack, block;
} FodderInput;

typedef struct {
    float x, z, yaw, vx, vz;
    float health, cooldown, windup;
    uint16_t squad;
    int16_t target;
    uint8_t side;
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

typedef struct {
    const FodderArena *arena;
    FodderVariant variant;
    FodderEffects effects;
    FodderSoldier soldiers[FODDER_SOLDIERS];
    int16_t grid[FODDER_GRID_SIDE * FODDER_GRID_SIDE];
    int16_t next[FODDER_SOLDIERS];
    float incoming[FODDER_SOLDIERS];
    unsigned count, starting_per_side, alive[2], tick, kills;
    uint32_t rng;
    float stamina, pitch, attack_time, swing;
    float hurt;
    bool guarding, dressed;
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
unsigned fodder_time_left(const FodderBattle *battle);

#endif
