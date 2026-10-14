#pragma once
#include "life_countdown_model.h"
#include <stdbool.h>
#include <stdint.h>

/* Fixed-size, versioned storage. Older days roll into totals; clock rollback
 * cannot reopen them. The completion order permits an exact last-action undo. */
#define LIFE_QUEST_MAGIC 0x4c513132U
typedef struct {
    uint32_t magic;
    uint16_t schema;
    uint8_t partner, name;
    life_date_t birth;
    uint8_t planning_age, sound, motion, reserved;
    int32_t latest_day;
    uint32_t archived_xp, archived_clears;
    uint8_t count, order[3], tipo_used, padding[3];
} life_quest_state_t;

typedef enum {
    LQ_OK = 0, LQ_DUPLICATE, LQ_CLOCK_ROLLBACK, LQ_UNAVAILABLE, LQ_INVALID
} life_quest_result_t;

void life_quest_default(life_quest_state_t *state);
bool life_quest_valid(const life_quest_state_t *state);
int32_t life_quest_day(life_date_t date);
uint32_t life_quest_xp(const life_quest_state_t *state);
uint8_t life_quest_hp(const life_quest_state_t *state, int32_t day);
uint8_t life_quest_count(const life_quest_state_t *state, int32_t day);
bool life_quest_done(const life_quest_state_t *state, int32_t day, uint8_t task);
life_quest_result_t life_quest_complete(life_quest_state_t *state, int32_t day,
                                       uint8_t task, uint8_t *reward);
life_quest_result_t life_quest_undo(life_quest_state_t *state, int32_t day,
                                   uint8_t *refund);
life_quest_result_t life_quest_tipo(life_quest_state_t *state, int32_t day);
uint8_t life_quest_china_percent(uint16_t age);
uint32_t life_quest_age_group(uint16_t age);
