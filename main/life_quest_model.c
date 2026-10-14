#include "life_quest_model.h"
#include "life_profile.h"
#include <stddef.h>
#include <string.h>

void life_quest_default(life_quest_state_t *s)
{
    memset(s, 0, sizeof(*s));
    s->magic = LIFE_QUEST_MAGIC;
    s->schema = 1;
    s->birth = (life_date_t){LIFE_PROFILE_BIRTH_YEAR, LIFE_PROFILE_BIRTH_MONTH,
                            LIFE_PROFILE_BIRTH_DAY};
    s->planning_age = LIFE_PROFILE_PLAN_AGE;
    s->sound = 1;
    s->motion = 1;
}

bool life_quest_valid(const life_quest_state_t *s)
{
    if (!s || s->magic != LIFE_QUEST_MAGIC || s->schema != 1 ||
        !life_countdown_date_is_valid(s->birth) || s->birth.year > 2099 ||
        s->planning_age < 50 || s->planning_age > 120 || s->partner > 1 ||
        s->name > 1 || s->sound > 1 || s->motion > 1 || s->count > 3 ||
        s->tipo_used > 1 || s->latest_day < 0 || s->latest_day > 47481 ||
        s->archived_xp > 1000000000U || s->archived_clears > 10000000U)
        return false;
    uint8_t mask = 0;
    for (unsigned i = 0; i < s->count; ++i) {
        if (s->order[i] > 2 || (mask & (1U << s->order[i]))) return false;
        mask |= 1U << s->order[i];
    }
    if (!s->latest_day && (s->count || s->tipo_used || s->archived_xp || s->archived_clears))
        return false;
    return true;
}

int32_t life_quest_day(life_date_t date)
{
    if (!life_countdown_date_is_valid(date)) return -1;
    int32_t y = date.year - (date.month <= 2);
    int32_t era = y / 400;
    uint32_t ye = (uint32_t)(y - era * 400);
    uint32_t month = date.month > 2 ? date.month - 3U : date.month + 9U;
    uint32_t dy = (153U * month + 2U) / 5U + date.day - 1U;
    return era * 146097 + (int32_t)(ye * 365U + ye / 4U - ye / 100U + dy)
           - 719468;
}

static uint32_t today_xp(const life_quest_state_t *s)
{
    return s->count * 20U + (s->count == 3 ? 40U : 0U);
}
uint32_t life_quest_xp(const life_quest_state_t *s)
{
    return s->archived_xp + today_xp(s);
}
uint8_t life_quest_count(const life_quest_state_t *s, int32_t day)
{
    return s->latest_day == day ? s->count : 0;
}
uint8_t life_quest_hp(const life_quest_state_t *s, int32_t day)
{
    int hp = 3 - life_quest_count(s, day) -
             (s->latest_day == day ? s->tipo_used : 0);
    return hp > 0 ? (uint8_t)hp : 0;
}
bool life_quest_done(const life_quest_state_t *s, int32_t day, uint8_t task)
{
    if (s->latest_day != day) return false;
    for (unsigned i = 0; i < s->count; ++i)
        if (s->order[i] == task) return true;
    return false;
}
static life_quest_result_t prepare_day(life_quest_state_t *s, int32_t day)
{
    if (!life_quest_valid(s) || day < 1 || day > 47481) return LQ_INVALID;
    if (day < s->latest_day) return LQ_CLOCK_ROLLBACK;
    if (day > s->latest_day) {
        uint32_t add = today_xp(s);
        if (s->archived_xp > 1000000000U - add) return LQ_UNAVAILABLE;
        s->archived_xp += add;
        s->archived_clears += s->count == 3;
        s->latest_day = day;
        s->count = s->tipo_used = 0;
        memset(s->order, 0, sizeof(s->order));
    }
    return LQ_OK;
}
life_quest_result_t life_quest_complete(life_quest_state_t *s, int32_t day,
                                       uint8_t task, uint8_t *reward)
{
    if (reward) *reward = 0;
    if (task > 2) return LQ_INVALID;
    life_quest_result_t result = prepare_day(s, day);
    if (result != LQ_OK) return result;
    if (life_quest_done(s, day, task)) return LQ_DUPLICATE;
    s->order[s->count++] = task;
    if (reward) *reward = s->count == 3 ? 60 : 20;
    return LQ_OK;
}
life_quest_result_t life_quest_undo(life_quest_state_t *s, int32_t day,
                                   uint8_t *refund)
{
    if (refund) *refund = 0;
    if (!life_quest_valid(s)) return LQ_INVALID;
    if (day < s->latest_day) return LQ_CLOCK_ROLLBACK;
    if (day != s->latest_day || s->count == 0) return LQ_UNAVAILABLE;
    if (refund) *refund = s->count == 3 ? 60 : 20;
    s->order[--s->count] = 0;
    return LQ_OK;
}
life_quest_result_t life_quest_tipo(life_quest_state_t *s, int32_t day)
{
    if (!life_quest_valid(s)) return LQ_INVALID;
    if (s->partner != 0) return LQ_UNAVAILABLE;
    if (day < s->latest_day) return LQ_CLOCK_ROLLBACK;
    if (day == s->latest_day && (s->tipo_used || life_quest_hp(s, day) == 0))
        return LQ_UNAVAILABLE;
    life_quest_result_t result = prepare_day(s, day);
    if (result == LQ_OK) s->tipo_used = 1;
    return result;
}

/* 2026-07-01 WPP 2024 medium projection, transcribed by
 * worldpopulationpyramid.com. Five-year within-bin interpolation is an
 * approximation, not a real personal ranking. No world percentile is known. */
static const uint32_t china_age[] = {
    45053929,73635394,90474554,85917946,78557362,81549874,92677026,
    121981256,105473092,92465202,109221765,118448579,103680631,65828081,
    67079098,42385015,22338449,11311571,4098001,684548,52713
};
uint8_t life_quest_china_percent(uint16_t age)
{
    if (age >= 100) return 99;
    uint64_t people5 = 0;
    for (unsigned i = 0; i < age / 5U; ++i) people5 += 5ULL * china_age[i];
    people5 += (age % 5U) * (uint64_t)china_age[age / 5U];
    return (uint8_t)(people5 * 100ULL / (5ULL * 1412914089ULL));
}
uint32_t life_quest_age_group(uint16_t age)
{
    return china_age[age >= 100 ? 20 : age / 5U];
}
