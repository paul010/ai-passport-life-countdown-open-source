#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
} life_date_t;

typedef struct {
    uint16_t age_years;
    uint32_t age_months;
    uint32_t lived_weeks;
    uint32_t lived_days;
    uint32_t remaining_days;
    uint8_t progress_percent;
    life_date_t planning_date;
} life_countdown_metrics_t;

bool life_countdown_date_is_valid(life_date_t date);
uint8_t life_countdown_days_in_month(uint16_t year, uint8_t month);
bool life_countdown_add_years(life_date_t date, uint16_t years,
                              life_date_t *result);
bool life_countdown_calculate(life_date_t birth_date, uint8_t planning_age,
                              life_date_t today,
                              life_countdown_metrics_t *metrics);
