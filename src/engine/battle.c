#include "battle.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

static const float step = 1.0f / FODDER_TICKS_PER_SECOND;
static const float pi = 3.14159265358979323846f;

float fodder_clamp(float value, float low, float high)
{
    return fminf(high, fmaxf(low, value));
}

float fodder_wrap(float angle)
{
    return remainderf(angle, 2.0f * pi);
}

static float random_unit(FodderBattle *battle)
{
    uint32_t bits = battle->rng;
    bits ^= bits << 13;
    bits ^= bits >> 17;
    bits ^= bits << 5;
    battle->rng = bits;
    return (float)(bits >> 8) / 16777216.0f;
}

float fodder_ground(const FodderArena *arena, float x, float z)
{
    float channel = fodder_clamp((arena->channel + 4.0f - fabsf(z)) / 4.0f, 0.0f, 1.0f);
    float ford = fodder_clamp((fabsf(x) - arena->ford + 2.0f) / 4.0f, 0.0f, 1.0f);
    return -arena->dip * channel * (0.18f + 0.82f * ford);
}

static void place_soldier(FodderBattle *battle, unsigned id, unsigned rank, unsigned side)
{
    FodderSoldier *soldier = &battle->soldiers[id];
    unsigned columns = battle->starting_per_side >= 60 ? 50 : 10;
    unsigned row = rank / columns;
    unsigned column = rank % columns;
    float width = battle->arena->half_width - 7.0f;
    float spacing = 2.0f * width / (float)columns;
    float direction = side == 0 ? 1.0f : -1.0f;
    soldier->side = (uint8_t)side;
    soldier->squad = (uint16_t)(rank / 10);
    soldier->x = -width + ((float)column + 0.5f) * spacing + (random_unit(battle) - 0.5f);
    soldier->z = direction * (12.0f + (float)row * 2.2f + random_unit(battle) * 1.1f);
    soldier->yaw = side == 0 ? 0.0f : pi;
    soldier->health = 100.0f;
    soldier->cooldown = random_unit(battle) * 0.4f;
    soldier->target = -1;
}

void fodder_battle_start(FodderBattle *battle, const FodderArena *arena,
                         const FodderVariant *variant, FodderEffects effects,
                         uint32_t seed, unsigned per_side)
{
    assert(per_side > 0 && per_side <= FODDER_ARMY_SIZE);
    memset(battle, 0, sizeof(*battle));
    battle->arena = arena;
    battle->variant = *variant;
    battle->effects = effects;
    battle->rng = seed ? seed : UINT32_C(277803737);
    battle->starting_per_side = per_side;
    battle->count = 2 * per_side;
    battle->alive[0] = per_side;
    battle->alive[1] = per_side;
    battle->stamina = 1.0f;
    battle->winner = -1;
    for (unsigned side = 0; side < 2; ++side) {
        for (unsigned rank = 0; rank < per_side; ++rank)
            place_soldier(battle, rank + side * per_side, rank, side);
    }
    FodderSoldier *player = &battle->soldiers[0];
    player->x = 0.0f;
    player->z = 15.5f;
    player->cooldown = 0.0f;
}

static bool inside(const FodderObstacle *obstacle, float x, float z)
{
    return fabsf(x - obstacle->x) < obstacle->half_x + 0.30f &&
           fabsf(z - obstacle->z) < obstacle->half_z + 0.30f;
}

static void move_soldier(const FodderArena *arena, FodderSoldier *soldier, float dx, float dz)
{
    float x = fodder_clamp(soldier->x + dx, -arena->half_width, arena->half_width);
    float z = fodder_clamp(soldier->z + dz, -arena->half_length, arena->half_length);
    for (unsigned i = 0; i < arena->obstacle_count; ++i) {
        const FodderObstacle *obstacle = &arena->obstacles[i];
        if (inside(obstacle, x, soldier->z))
            x = soldier->x;
        if (inside(obstacle, x, z))
            z = soldier->z;
    }
    soldier->x = x;
    soldier->z = z;
}

static float distance_squared(const FodderSoldier *a, const FodderSoldier *b)
{
    float dx = a->x - b->x;
    float dz = a->z - b->z;
    return dx * dx + dz * dz;
}

static int cell(float coordinate)
{
    return (int)fodder_clamp(floorf(coordinate / 4.0f) + FODDER_GRID_SIDE / 2,
                             0.0f, FODDER_GRID_SIDE - 1.0f);
}

static void make_grid(FodderBattle *battle)
{
    for (unsigned i = 0; i < FODDER_GRID_SIDE * FODDER_GRID_SIDE; ++i)
        battle->grid[i] = -1;
    for (unsigned i = 0; i < battle->count; ++i) {
        const FodderSoldier *soldier = &battle->soldiers[i];
        if (soldier->health <= 0)
            continue;
        int index = cell(soldier->z) * FODDER_GRID_SIDE + cell(soldier->x);
        battle->next[i] = battle->grid[index];
        battle->grid[index] = (int16_t)i;
    }
}

static float facing(const FodderSoldier *observer, const FodderSoldier *other)
{
    return sinf(observer->yaw) * (other->x - observer->x) -
           cosf(observer->yaw) * (other->z - observer->z);
}

static int find_opponent(const FodderBattle *battle, unsigned id, float radius, bool cone)
{
    const FodderSoldier *soldier = &battle->soldiers[id];
    float best_distance = radius * radius;
    int best = -1;
    int min_z = cell(soldier->z - radius);
    int max_z = cell(soldier->z + radius);
    int min_x = cell(soldier->x - radius);
    int max_x = cell(soldier->x + radius);
    for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
            for (int i = battle->grid[z * FODDER_GRID_SIDE + x]; i >= 0; i = battle->next[i]) {
                const FodderSoldier *other = &battle->soldiers[i];
                if (other->side == soldier->side)
                    continue;
                float distance = distance_squared(soldier, other);
                if (distance >= best_distance)
                    continue;
                if (cone && facing(soldier, other) <= sqrtf(distance) * 0.48f)
                    continue;
                best = i;
                best_distance = distance;
            }
        }
    }
    return best;
}

static bool clear_between(const FodderArena *arena, const FodderSoldier *a, const FodderSoldier *b)
{
    float x = (a->x + b->x) * 0.5f;
    float z = (a->z + b->z) * 0.5f;
    for (unsigned i = 0; i < arena->obstacle_count; ++i) {
        if (inside(&arena->obstacles[i], x, z))
            return false;
    }
    return true;
}

static void read_player(FodderBattle *battle, FodderInput input)
{
    FodderSoldier *player = &battle->soldiers[0];
    player->yaw = fodder_wrap(player->yaw + input.look_x);
    battle->pitch = fodder_clamp(battle->pitch + input.look_y, -1.32f, 1.32f);
    battle->guarding = input.block && battle->stamina > 0.04f && battle->swing <= 0.0f;
    battle->stamina = fminf(1.0f, battle->stamina + step * battle->effects.recovery *
                            (battle->guarding ? 0.10f : 0.29f));
    battle->hurt = fmaxf(0.0f, battle->hurt - step * 2.9f);
    float move = fodder_clamp(input.move, -1.0f, 1.0f);
    float strafe = fodder_clamp(input.strafe, -1.0f, 1.0f);
    float scale = (battle->guarding ? 2.2f : 4.9f) / fmaxf(1.0f, hypotf(move, strafe));
    float dx = (sinf(player->yaw) * move + cosf(player->yaw) * strafe) * scale;
    float dz = (-cosf(player->yaw) * move + sinf(player->yaw) * strafe) * scale;
    player->vx = dx;
    player->vz = dz;
    move_soldier(battle->arena, player, dx * step, dz * step);

    battle->attack_time = fmaxf(0.0f, battle->attack_time - step);
    if (input.attack && battle->attack_time == 0.0f && !battle->guarding &&
        battle->stamina >= 0.18f) {
        battle->attack_time = 0.56f;
        battle->swing = 0.145f;
        battle->stamina -= 0.18f;
    }
}

static void player_hit(FodderBattle *battle, int *victim)
{
    if (battle->swing <= 0.0f)
        return;
    battle->swing -= step;
    if (battle->swing > 0.0f)
        return;
    int target = find_opponent(battle, 0, 2.65f, true);
    if (target >= 0 && clear_between(battle->arena, battle->soldiers, &battle->soldiers[target])) {
        battle->incoming[target] += battle->variant.player_damage;
        *victim = target;
    }
}

static void hit(FodderBattle *battle, FodderSoldier *soldier)
{
    FodderSoldier *target = &battle->soldiers[soldier->target];
    if (target->health <= 0.0f || distance_squared(soldier, target) > 2.85f * 2.85f ||
        !clear_between(battle->arena, soldier, target))
        return;
    float damage = soldier->side == 0 ? battle->variant.ally_damage : battle->variant.enemy_damage;
    if (soldier->target == 0) {
        damage *= 2.65f;
        float cost = 0.17f * battle->effects.block_cost;
        if (battle->guarding && battle->stamina >= cost && facing(target, soldier) > 0.0f) {
            battle->stamina -= cost;
            damage *= 0.12f;
        }
        battle->hurt = fmaxf(battle->hurt, damage / 23.0f);
    }
    battle->incoming[soldier->target] += damage;
}

static void follow(FodderBattle *battle, FodderSoldier *soldier, float goal_x, float goal_z)
{
    float dx = goal_x - soldier->x;
    float dz = goal_z - soldier->z;
    float length = hypotf(dx, dz);
    float speed = soldier->side == 0 ? battle->variant.ally_speed : battle->variant.enemy_speed;
    if (length < 0.15f) {
        soldier->vx = 0;
        soldier->vz = 0;
        return;
    }
    soldier->yaw = atan2f(dx, -dz);
    soldier->vx = (dx / length) * speed;
    soldier->vz = (dz / length) * speed;
}

static void choose_action(FodderBattle *battle, unsigned id)
{
    FodderSoldier *soldier = &battle->soldiers[id];
    int target = find_opponent(battle, id, 7.5f, false);
    if (target >= 0) {
        FodderSoldier *enemy = &battle->soldiers[target];
        float distance = distance_squared(soldier, enemy);
        if (distance > 2.00f * 2.00f || !clear_between(battle->arena, soldier, enemy)) {
            follow(battle, soldier, enemy->x, enemy->z);
            return;
        }
        soldier->vx = soldier->vz = 0.0f;
        soldier->yaw = atan2f(enemy->x - soldier->x, soldier->z - enemy->z);
        if (soldier->cooldown <= 0.0f) {
            soldier->target = (int16_t)target;
            soldier->windup = 0.28f + random_unit(battle) * 0.18f;
            soldier->cooldown = 0.93f + random_unit(battle) * 0.70f;
        }
        return;
    }
    float lane = (float)(soldier->side == 0 ? battle->variant.ally_lane : battle->variant.enemy_lane);
    float variation = ((float)(soldier->squad % 5) - 2.0f) * 0.36f;
    float to_z = soldier->side == 0 ? -battle->arena->half_length : battle->arena->half_length;
    float to_x = fodder_clamp(soldier->x + lane * 1.4f + variation,
                              -battle->arena->half_width + 2.0f, battle->arena->half_width - 2.0f);
    follow(battle, soldier, to_x, to_z);
}

static void run_ai(FodderBattle *battle, unsigned id)
{
    FodderSoldier *soldier = &battle->soldiers[id];
    if (soldier->health <= 0.0f)
        return;
    soldier->cooldown -= step;
    if (soldier->windup > 0.0f) {
        soldier->windup -= step;
        if (soldier->windup <= 0.0f)
            hit(battle, soldier);
        return;
    }

    unsigned interval = distance_squared(soldier, battle->soldiers) < 26.0f * 26.0f ? 3u : 12u;
    if ((battle->tick + id) % interval == 0)
        choose_action(battle, id);

    float old_z = soldier->z;
    move_soldier(battle->arena, soldier, soldier->vx * step, soldier->vz * step);
    if (old_z == soldier->z && fabsf(soldier->vz) > 0.05f) {
        float direction = soldier->squad % 2 ? -1.0f : 1.0f;
        move_soldier(battle->arena, soldier, direction * fabsf(soldier->vz) * step, 0);
    }
}

static void apply_damage(FodderBattle *battle, int victim)
{
    for (unsigned i = 0; i < battle->count; ++i) {
        FodderSoldier *soldier = &battle->soldiers[i];
        if (battle->incoming[i] <= 0.0f || soldier->health <= 0.0f)
            continue;
        soldier->health = fmaxf(0.0f, soldier->health - battle->incoming[i]);
        if (i == 0 && soldier->health > 0.0f && soldier->health < 32.0f &&
            !battle->dressed && battle->effects.dressing > 0.0f) {
            soldier->health += battle->effects.dressing;
            battle->dressed = true;
        }
        if (soldier->health == 0.0f) {
            --battle->alive[soldier->side];
            if ((int)i == victim)
                ++battle->kills;
        }
    }
}

static void resolve_outcome(FodderBattle *battle)
{
    unsigned rout = (unsigned)((float)battle->starting_per_side * battle->variant.rout_fraction);
    if (battle->alive[0] <= rout || battle->alive[1] <= rout)
        battle->end = FODDER_ROUT;
    else if (fodder_time_left(battle) == 0)
        battle->end = FODDER_HORN;
    else if (battle->soldiers[0].health <= 0.0f)
        battle->end = FODDER_DEATH;
    if (battle->end == FODDER_ONGOING)
        return;
    if (battle->end != FODDER_DEATH && battle->alive[0] != battle->alive[1])
        battle->winner = battle->alive[0] > battle->alive[1] ? 0 : 1;
    battle->outcome = battle->soldiers[0].health > 0.0f ? FODDER_SURVIVED : FODDER_FALLEN;
}

void fodder_battle_tick(FodderBattle *battle, FodderInput input)
{
    if (battle->outcome != FODDER_ACTIVE)
        return;
    ++battle->tick;
    memset(battle->incoming, 0, battle->count * sizeof(battle->incoming[0]));
    read_player(battle, input);
    make_grid(battle);
    int victim = -1;
    player_hit(battle, &victim);
    for (unsigned i = 1; i < battle->count; ++i)
        run_ai(battle, i);
    apply_damage(battle, victim);
    resolve_outcome(battle);
}

unsigned fodder_time_left(const FodderBattle *battle)
{
    unsigned elapsed = battle->tick / FODDER_TICKS_PER_SECOND;
    return elapsed < battle->variant.seconds ? battle->variant.seconds - elapsed : 0;
}
