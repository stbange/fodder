#include "profile.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

bool fodder_profile_decode(FodderProfile *profile, const char *text, unsigned variants)
{
    bool number = false;
    unsigned values[12] = {0};
    unsigned index = 0;
    bool legacy = strncmp(text, "FODDER1 ", 8) == 0;
    if (!legacy && strncmp(text, "FODDER2 ", 8) != 0)
        return false;
    unsigned last = legacy ? 10 : 11;
    for (const char *p = text + 8; *p; ++p) {
        if (*p == ' ' && number && index < last) {
            ++index;
            number = false;
            continue;
        }
        if (*p < '0' || *p > '9')
            return false;
        unsigned digit = (unsigned)(*p - '0');
        if (values[index] > (UINT_MAX - digit) / 10u)
            return false;
        values[index] = values[index] * 10u + digit;
        number = true;
    }
    if (index != last || !number || !variants || values[10] > 1 || values[11] > values[10] ||
        values[2] >= FODDER_VARIANTS || values[6] > FODDER_ARMY_SIZE ||
        values[9] > values[6] || values[7] > values[8] || values[8] > values[1] ||
        values[0] < values[1] || (values[10] && !values[7]))
        return false;
    *profile = (FodderProfile){
        .revision = values[0], .attempts = values[1], .variant = values[2] % variants,
        .rank = {values[3], values[4], values[5]},
        .best_kills = values[6], .streak = values[7], .best_streak = values[8],
        .last_kills = values[9], .pending = values[10] != 0, .bonus = values[11] != 0
    };
    return true;
}

bool fodder_profile_encode(const FodderProfile *profile, char *text, size_t length)
{
    int used = snprintf(text, length, "FODDER2 %u %u %u %u %u %u %u %u %u %u %u %u",
                        profile->revision, profile->attempts, profile->variant,
                        profile->rank[0], profile->rank[1], profile->rank[2],
                        profile->best_kills, profile->streak, profile->best_streak,
                        profile->last_kills, (unsigned)profile->pending, (unsigned)profile->bonus);
    return used > 0 && (size_t)used < length;
}

bool fodder_profile_result(FodderProfile *next, const FodderProfile *profile,
                           FodderOutcome outcome, unsigned kills, unsigned deeds)
{
    if (outcome == FODDER_ACTIVE || profile->pending || kills > FODDER_ARMY_SIZE ||
        profile->revision >= UINT_MAX - 1 || profile->attempts == UINT_MAX)
        return false;
    *next = *profile;
    ++next->revision;
    ++next->attempts;
    next->last_kills = kills;
    if (kills > next->best_kills)
        next->best_kills = kills;
    next->pending = outcome == FODDER_SURVIVED;
    next->bonus = next->pending && deeds >= FODDER_DEEDS_FOR_BONUS;
    next->streak = next->pending ? next->streak + 1 : 0;
    if (next->streak > next->best_streak)
        next->best_streak = next->streak;
    return true;
}

bool fodder_profile_claim(FodderProfile *next, const FodderProfile *profile,
                          unsigned upgrade, unsigned variants)
{
    unsigned ranks = fodder_profile_reward(profile);
    if (!ranks || upgrade >= FODDER_UPGRADES || variants == 0 ||
        profile->revision == UINT_MAX || profile->rank[upgrade] > UINT_MAX - ranks)
        return false;
    *next = *profile;
    next->pending = false;
    next->bonus = false;
    ++next->revision;
    next->rank[upgrade] += ranks;
    next->variant = (next->variant + 1) % variants;
    return true;
}

unsigned fodder_profile_reward(const FodderProfile *profile)
{
    return profile->pending ? 1u + (unsigned)profile->bonus : 0u;
}

uint32_t fodder_profile_seed(const FodderProfile *profile)
{
    return UINT32_C(1249337101) ^ (uint32_t)profile->attempts * UINT32_C(2654435761);
}
