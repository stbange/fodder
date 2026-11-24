#include "battle.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

static const float step = 1.0f / FODDER_TICKS_PER_SECOND;
static const float pi = 3.14159265358979323846f;
static const float body_diameter = 0.70f;
static const float player_reach = 2.30f;
static const float strike_contact = 0.42f;

enum { FLANK_TO_REAR = 1, FLANK_SWEEP_REAR };

static const struct {
    float windup, recovery, arc, damage, guard_cost;
} strikes[] = {
    [FODDER_STRIKE_SLASH] = {0.45f, 0.38f, 0.55f, 1.0f, 0.17f},
    [FODDER_STRIKE_OVERHEAD] = {0.65f, 0.54f, 0.83f, 1.18f, 0.34f}
};

float fodder_clamp(float value, float low, float high)
{
    return fminf(high, fmaxf(low, value));
}

float fodder_wrap(float angle)
{
    return remainderf(angle, 2.0f * pi);
}

float fodder_bearing(const FodderSoldier *observer, const FodderSoldier *other)
{
    return fodder_wrap(atan2f(other->x - observer->x, observer->z - other->z) - observer->yaw);
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

static void place_soldier(FodderBattle *battle, unsigned id, unsigned rank, unsigned side,
                           unsigned flankers)
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
    soldier->strike = FODDER_STRIKE_OVERHEAD;

    if (side == 1 && flankers && rank >= battle->starting_per_side - 2 * flankers) {
        unsigned wing_rank = rank - (battle->starting_per_side - 2 * flankers);
        unsigned position = wing_rank % flankers;
        float direction_x = wing_rank < flankers ? -1.0f : 1.0f;
        soldier->x = direction_x * (battle->arena->half_width - 1.4f - 0.96f * (float)(position % 3));
        soldier->z = -13.0f - 1.14f * (float)(position / 3);
        soldier->flank_stage = FLANK_TO_REAR;
    }
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
    battle->counter_target = -1;
    battle->counter_swing = -1;
    battle->attacker = -1;
    battle->flanker = -1;
    unsigned flankers = variant->flankers < per_side / 8 ? variant->flankers : per_side / 8;
    for (unsigned side = 0; side < 2; ++side) {
        for (unsigned rank = 0; rank < per_side; ++rank)
            place_soldier(battle, rank + side * per_side, rank, side, flankers);
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

static int grid_index(float x, float z)
{
    return cell(z) * FODDER_GRID_SIDE + cell(x);
}

static void insert_soldier(FodderBattle *battle, unsigned id, int index)
{
    int head = battle->grid[index];
    battle->previous[id] = -1;
    battle->next[id] = (int16_t)head;
    if (head >= 0)
        battle->previous[head] = (int16_t)id;
    battle->cells[id] = (int16_t)index;
    battle->grid[index] = (int16_t)id;
}

static void make_grid(FodderBattle *battle)
{
    for (unsigned i = 0; i < FODDER_GRID_SIDE * FODDER_GRID_SIDE; ++i)
        battle->grid[i] = -1;
    memset(battle->strikes_at, 0, battle->count * sizeof(battle->strikes_at[0]));
    for (unsigned i = 0; i < battle->count; ++i) {
        const FodderSoldier *soldier = &battle->soldiers[i];
        if (soldier->health <= 0.0f)
            continue;
        insert_soldier(battle, i, grid_index(soldier->x, soldier->z));
        if (soldier->target >= 0 && (soldier->windup > 0.0f || soldier->recovery > 0.0f))
            ++battle->strikes_at[soldier->target];
    }
}

static void update_cell(FodderBattle *battle, unsigned id)
{
    const FodderSoldier *soldier = &battle->soldiers[id];
    int index = grid_index(soldier->x, soldier->z);
    int previous_index = battle->cells[id];
    if (index == previous_index)
        return;
    int previous = battle->previous[id], next = battle->next[id];
    if (previous >= 0)
        battle->next[previous] = (int16_t)next;
    else
        battle->grid[previous_index] = (int16_t)next;
    if (next >= 0)
        battle->previous[next] = (int16_t)previous;
    insert_soldier(battle, id, index);
}

static bool path_open(const FodderBattle *battle, unsigned id, float x, float z)
{
    const FodderSoldier *mover = &battle->soldiers[id];
    for (unsigned i = 0; i < battle->arena->obstacle_count; ++i) {
        if (inside(&battle->arena->obstacles[i], x, z))
            return false;
    }

    int min_z = cell(z - body_diameter), max_z = cell(z + body_diameter);
    int min_x = cell(x - body_diameter), max_x = cell(x + body_diameter);
    for (int row = min_z; row <= max_z; ++row) {
        for (int column = min_x; column <= max_x; ++column) {
            for (int i = battle->grid[row * FODDER_GRID_SIDE + column]; i >= 0; i = battle->next[i]) {
                if ((unsigned)i == id)
                    continue;
                const FodderSoldier *other = &battle->soldiers[i];
                float dx = x - other->x, dz = z - other->z;
                float separation = dx * dx + dz * dz;
                if (separation < body_diameter * body_diameter && separation <= distance_squared(mover, other))
                    return false;
            }
        }
    }
    return true;
}

static bool move_soldier(FodderBattle *battle, unsigned id, float dx, float dz)
{
    FodderSoldier *soldier = &battle->soldiers[id];
    float start_x = soldier->x, start_z = soldier->z;
    float x = fodder_clamp(start_x + dx, -battle->arena->half_width, battle->arena->half_width);
    float z = fodder_clamp(start_z + dz, -battle->arena->half_length, battle->arena->half_length);
    if (x == start_x && z == start_z)
        return false;

    if (path_open(battle, id, x, z)) {
        soldier->x = x;
        soldier->z = z;
    } else {
        if (x != start_x && path_open(battle, id, x, start_z))
            soldier->x = x;
        if (z != start_z && path_open(battle, id, soldier->x, z))
            soldier->z = z;
    }
    update_cell(battle, id);
    return soldier->x != start_x || soldier->z != start_z;
}

static float facing(const FodderSoldier *observer, const FodderSoldier *other)
{
    return sinf(observer->yaw) * (other->x - observer->x) -
           cosf(observer->yaw) * (other->z - observer->z);
}

static bool clip_segment(float origin, float delta, float bounds_min, float bounds_max,
                         float *first, float *last)
{
    if (fabsf(delta) < 0.0001f)
        return origin >= bounds_min && origin <= bounds_max;
    float a = (bounds_min - origin) / delta, b = (bounds_max - origin) / delta;
    *first = fmaxf(*first, fminf(a, b));
    *last = fminf(*last, fmaxf(a, b));
    return *first <= *last;
}

static bool clear_strike(const FodderBattle *battle, unsigned id, int target)
{
    const FodderSoldier *a = &battle->soldiers[id];
    const FodderSoldier *b = target >= 0 ? &battle->soldiers[target] : NULL;
    float vx = b ? b->x - a->x : sinf(a->yaw) * player_reach;
    float vz = b ? b->z - a->z : -cosf(a->yaw) * player_reach;
    for (unsigned i = 0; i < battle->arena->obstacle_count; ++i) {
        const FodderObstacle *o = &battle->arena->obstacles[i];
        float first = 0.0f, last = 1.0f;
        if (clip_segment(a->x, vx, o->x - o->half_x, o->x + o->half_x, &first, &last) &&
            clip_segment(a->z, vz, o->z - o->half_z, o->z + o->half_z, &first, &last))
            return false;
    }

    float length = vx * vx + vz * vz;
    if (length < 0.0001f)
        return true;
    const float radius = body_diameter * 0.42f;
    int min_x = cell(fminf(a->x, a->x + vx) - radius), max_x = cell(fmaxf(a->x, a->x + vx) + radius);
    int min_z = cell(fminf(a->z, a->z + vz) - radius), max_z = cell(fmaxf(a->z, a->z + vz) + radius);
    for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
            for (int i = battle->grid[z * FODDER_GRID_SIDE + x]; i >= 0; i = battle->next[i]) {
                if ((unsigned)i == id || i == target)
                    continue;
                const FodderSoldier *body = &battle->soldiers[i];
                float dx = body->x - a->x, dz = body->z - a->z;
                float along = (dx * vx + dz * vz) / length;
                if (along > 0.0f && along < 1.0f) {
                    float across_x = dx - along * vx, across_z = dz - along * vz;
                    if (across_x * across_x + across_z * across_z < radius * radius)
                        return false;
                }
            }
        }
    }
    return true;
}

static int find_opponent(const FodderBattle *battle, unsigned id, float radius, bool aimed)
{
    const FodderSoldier *soldier = &battle->soldiers[id];
    float reach = radius * radius, best_distance = reach;
    float best_aim = 0.75f;
    float forward_x = aimed ? sinf(soldier->yaw) : 0.0f;
    float forward_z = aimed ? -cosf(soldier->yaw) : 0.0f;
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
                if (distance >= reach)
                    continue;
                if (aimed) {
                    float projection = forward_x * (other->x - soldier->x) + forward_z * (other->z - soldier->z);
                    float aim = projection / fmaxf(0.001f, sqrtf(distance));
                    if (aim < best_aim || (aim == best_aim && distance >= best_distance))
                        continue;
                    best_aim = aim;
                } else if (distance >= best_distance) {
                    continue;
                }
                best = i;
                best_distance = distance;
            }
        }
    }
    return best;
}

static void begin_strike(FodderSoldier *soldier, FodderStrike strike, float duration)
{
    soldier->guarding = false;
    soldier->strike = (uint8_t)strike;
    soldier->windup_duration = duration;
    soldier->windup = duration;
}

static bool advance_strike(FodderSoldier *soldier)
{
    if (soldier->windup > 0.0f) {
        soldier->windup = fmaxf(0.0f, soldier->windup - step);
        if (soldier->windup < 0.00001f) {
            soldier->windup = 0.0f;
            soldier->recovery = strikes[soldier->strike].recovery;
            return true;
        }
    } else {
        soldier->recovery = fmaxf(0.0f, soldier->recovery - step);
    }
    return false;
}

float fodder_strike_phase(const FodderSoldier *soldier)
{
    if (soldier->windup > 0.0f)
        return strike_contact * (1.0f - soldier->windup / fmaxf(soldier->windup, soldier->windup_duration));
    if (soldier->recovery > 0.0f)
        return 1.0f - (1.0f - strike_contact) * soldier->recovery / strikes[soldier->strike].recovery;
    return 0.0f;
}

static void feedback(FodderBattle *battle, FodderCombatFeedback signal)
{
    battle->feedback = signal;
    battle->feedback_tick = battle->tick;
    battle->feedback_time = 0.60f;
}

static void read_player(FodderBattle *battle, FodderInput input)
{
    FodderSoldier *player = &battle->soldiers[0];
    player->yaw = fodder_wrap(player->yaw + input.look_x);
    battle->pitch = fodder_clamp(battle->pitch + input.look_y, -1.32f, 1.32f);
    battle->guard_window = fmaxf(0.0f, battle->guard_window - step);
    battle->guard_rearm = fmaxf(0.0f, battle->guard_rearm - step);
    battle->counter_time = fmaxf(0.0f, battle->counter_time - step);
    battle->feedback_time = fmaxf(0.0f, battle->feedback_time - step);
    player->guarding = input.block && battle->stamina > 0.04f &&
                       player->windup == 0.0f && player->recovery <= 0.16f;
    if (!player->guarding)
        battle->guard_window = 0.0f;
    if (player->guarding && !battle->block_held && battle->guard_rearm == 0.0f) {
        battle->guard_window = 0.22f;
        battle->guard_rearm = 0.48f;
    }
    battle->block_held = input.block;

    bool pressed = input.attack_pressed || (input.attack && !battle->attack_held);
    battle->attack_held = input.attack;
    if (pressed && player->windup == 0.0f && player->recovery == 0.0f &&
        !player->guarding && battle->stamina >= 0.18f) {
        battle->counter_swing = fodder_counter_opponent(battle);
        begin_strike(player, FODDER_STRIKE_SLASH, 0.24f);
        battle->sword_recoil = false;
        battle->stamina -= 0.18f;
    }

    battle->stamina = fminf(1.0f, battle->stamina + step * battle->effects.recovery *
                           (player->guarding ? 0.08f : 0.29f));
    battle->hurt = fmaxf(0.0f, battle->hurt - step * 2.9f);
    float move = fodder_clamp(input.move, -1.0f, 1.0f);
    float strafe = fodder_clamp(input.strafe, -1.0f, 1.0f);
    float speed = player->guarding ? 2.2f : (player->windup > 0.0f ? 3.25f : 4.9f);
    float scale = speed / fmaxf(1.0f, hypotf(move, strafe));
    float dx = (sinf(player->yaw) * move + cosf(player->yaw) * strafe) * scale;
    float dz = (-cosf(player->yaw) * move + sinf(player->yaw) * strafe) * scale;
    float old_x = player->x, old_z = player->z;
    move_soldier(battle, 0, dx * step, dz * step);
    player->vx = (player->x - old_x) / step;
    player->vz = (player->z - old_z) / step;
}

static bool front_guard(const FodderSoldier *target, const FodderSoldier *attacker)
{
    return target->guarding && facing(target, attacker) > 0.52f * sqrtf(distance_squared(target, attacker));
}

static int counter_in_aim(const FodderBattle *battle, int target)
{
    if (target < 0)
        return -1;
    const FodderSoldier *player = battle->soldiers, *enemy = &battle->soldiers[target];
    float range = distance_squared(player, enemy);
    return enemy->health > 0.0f && range < player_reach * player_reach &&
           facing(player, enemy) > sqrtf(range) * 0.70f ? target : -1;
}

int fodder_counter_opponent(const FodderBattle *battle)
{
    if (battle->soldiers[0].windup > 0.0f)
        return counter_in_aim(battle, battle->counter_swing);
    return battle->counter_time > 0.0f ? counter_in_aim(battle, battle->counter_target) : -1;
}

static int player_hit(FodderBattle *battle, bool *countered)
{
    FodderSoldier *player = &battle->soldiers[0];
    if (player->health <= 0.0f || !advance_strike(player))
        return -1;
    int counter = battle->counter_swing;
    int victim = counter >= 0 ? counter_in_aim(battle, counter) : find_opponent(battle, 0, player_reach, true);
    if ((counter < 0 || victim >= 0) && !clear_strike(battle, 0, victim)) {
        battle->sword_recoil = true;
        feedback(battle, FODDER_COMBAT_OBSTRUCTED);
        return -1;
    }
    if (victim < 0) {
        feedback(battle, FODDER_COMBAT_MISS);
        return -1;
    }

    FodderSoldier *target = &battle->soldiers[victim];
    target->provoked_until = battle->tick + 3 * FODDER_TICKS_PER_SECOND;
    float damage = battle->variant.player_damage;
    *countered = counter >= 0;
    if (*countered) {
        damage *= 2.15f;
        battle->counter_time = 0.0f;
        battle->stamina = fminf(1.0f, battle->stamina + 0.25f);
        ++battle->deeds;
        feedback(battle, FODDER_COMBAT_COUNTER);
    } else if (front_guard(target, player)) {
        damage *= 0.26f;
        battle->sword_recoil = true;
        feedback(battle, FODDER_COMBAT_BLOCKED);
    } else {
        feedback(battle, FODDER_COMBAT_HIT);
    }
    battle->incoming[victim] += damage;
    return victim;
}

static void hit(FodderBattle *battle, unsigned id)
{
    FodderSoldier *soldier = &battle->soldiers[id];
    if (soldier->target < 0)
        return;
    FodderSoldier *target = &battle->soldiers[soldier->target];
    float distance = distance_squared(soldier, target);
    if (target->health <= 0.0f || distance > 2.05f * 2.05f ||
        facing(soldier, target) <= sqrtf(distance) * strikes[soldier->strike].arc ||
        !clear_strike(battle, id, soldier->target))
        return;
    float damage = soldier->side == 0 ? battle->variant.ally_damage : battle->variant.enemy_damage;
    if (soldier->target == 0) {
        damage *= 2.65f * strikes[soldier->strike].damage;
        float cost = strikes[soldier->strike].guard_cost * battle->effects.block_cost;
        float parry_cost = 0.06f * battle->effects.block_cost;
        if (front_guard(target, soldier)) {
            if (battle->guard_window > 0.0f && battle->stamina >= parry_cost) {
                battle->stamina -= parry_cost;
                battle->guard_window = 0.0f;
                battle->counter_time = 0.76f;
                battle->counter_target = (int)id;
                target->recovery = 0.0f;
                soldier->stagger = 0.46f;
                feedback(battle, FODDER_COMBAT_PARRY);
                return;
            }
            if (battle->stamina >= cost) {
                battle->stamina -= cost;
                damage *= 0.18f;
                feedback(battle, FODDER_COMBAT_GUARDED);
            } else {
                battle->stamina = 0.0f;
                target->guarding = false;
                feedback(battle, FODDER_COMBAT_GUARD_BROKEN);
            }
        }
        battle->hurt = fmaxf(battle->hurt, damage / 23.0f);
    } else if (front_guard(target, soldier)) {
        damage *= 0.72f;
    }
    battle->incoming[soldier->target] += damage;
}

static void follow(FodderBattle *battle, FodderSoldier *soldier, float goal_x, float goal_z)
{
    float dx = goal_x - soldier->x;
    float dz = goal_z - soldier->z;
    float length = hypotf(dx, dz);
    float speed = soldier->side == 0 ? battle->variant.ally_speed : battle->variant.enemy_speed;
    if (soldier->flank_stage)
        speed = fminf(7.0f, speed * 1.78f);
    soldier->guarding = false;
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
    if (soldier->flank_stage == FLANK_TO_REAR) {
        float crossing = 0.68f * battle->arena->half_length;
        if (soldier->z < crossing - 0.2f) {
            follow(battle, soldier, soldier->x, crossing);
            return;
        }
        soldier->flank_stage = FLANK_SWEEP_REAR;
    }

    bool provoked = soldier->provoked_until > battle->tick &&
                    distance_squared(soldier, battle->soldiers) < 3.2f * 3.2f;
    int target = provoked ? 0 : find_opponent(battle, id, 7.5f, false);
    if (target >= 0) {
        FodderSoldier *enemy = &battle->soldiers[target];
        float distance = distance_squared(soldier, enemy);
        if (distance > 1.75f * 1.75f || !clear_strike(battle, id, target)) {
            follow(battle, soldier, enemy->x, enemy->z);
            return;
        }
        soldier->vx = soldier->vz = 0.0f;
        soldier->yaw = atan2f(enemy->x - soldier->x, soldier->z - enemy->z);
        unsigned limit = target == 0 ? 1u : 2u;
        if (soldier->cooldown <= 0.0f && battle->strikes_at[target] < limit) {
            soldier->target = (int16_t)target;
            ++battle->strikes_at[target];
            FodderStrike strike = enemy->guarding || soldier->strike == FODDER_STRIKE_SLASH ?
                                  FODDER_STRIKE_OVERHEAD : FODDER_STRIKE_SLASH;
            begin_strike(soldier, strike, strikes[strike].windup);
            if (target == 0)
                battle->attacker = (int)id;
            soldier->cooldown = 0.98f + random_unit(battle) * 0.70f;
        } else {
            soldier->guarding = true;
        }
        return;
    }

    if (soldier->flank_stage == FLANK_SWEEP_REAR) {
        const FodderSoldier *player = &battle->soldiers[0];
        float rear = 0.24f * battle->arena->half_length;
        follow(battle, soldier, player->z >= rear ? player->x : 0.0f, fmaxf(rear, player->z));
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
    float player_distance = distance_squared(soldier, battle->soldiers);
    if (soldier->flank_stage == FLANK_SWEEP_REAR && player_distance < 22.0f * 22.0f &&
        (battle->flanker < 0 || player_distance < distance_squared(&battle->soldiers[battle->flanker], battle->soldiers)))
        battle->flanker = (int)id;
    if (soldier->stagger > 0.0f) {
        soldier->stagger = fmaxf(0.0f, soldier->stagger - step);
        return;
    }
    if (soldier->windup > 0.0f || soldier->recovery > 0.0f) {
        if (advance_strike(soldier))
            hit(battle, id);
        else if (soldier->target == 0 && soldier->windup > 0.0f)
            battle->attacker = (int)id;
        return;
    }

    unsigned interval = player_distance < 26.0f * 26.0f ? 3u : 12u;
    if ((battle->tick + id) % interval == 0)
        choose_action(battle, id);

    float dx = soldier->vx * step, dz = soldier->vz * step;
    if (fabsf(dx) + fabsf(dz) < 0.001f || move_soldier(battle, id, dx, dz))
        return;
    float sidestep = soldier->squad % 2 ? -0.85f : 0.85f;
    if (!move_soldier(battle, id, -dz * sidestep, dx * sidestep))
        move_soldier(battle, id, dz * sidestep, -dx * sidestep);
}

static void apply_damage(FodderBattle *battle, int victim, bool countered)
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
            if ((int)i == victim) {
                ++battle->kills;
                if (!countered)
                    ++battle->deeds;
            }
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
    battle->attacker = -1;
    battle->flanker = -1;
    memset(battle->incoming, 0, battle->count * sizeof(battle->incoming[0]));
    make_grid(battle);
    if (battle->soldiers[0].health > 0.0f)
        read_player(battle, input);
    bool countered = false;
    int victim = player_hit(battle, &countered);
    for (unsigned i = 1; i < battle->count; ++i)
        run_ai(battle, i);
    apply_damage(battle, victim, countered);
    resolve_outcome(battle);
}

unsigned fodder_time_left(const FodderBattle *battle)
{
    unsigned elapsed = battle->tick / FODDER_TICKS_PER_SECOND;
    return elapsed < battle->variant.seconds ? battle->variant.seconds - elapsed : 0;
}
