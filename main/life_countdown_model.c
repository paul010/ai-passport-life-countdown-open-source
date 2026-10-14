#include "life_countdown_model.h"

#include <stddef.h>

static bool is_leap_year(uint16_t year)
{
    return (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
}

uint8_t life_countdown_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    if (month < 1U || month > 12U) {
        return 0U;
    }
    if (month == 2U && is_leap_year(year)) {
        return 29U;
    }
    return days[month - 1U];
}

bool life_countdown_date_is_valid(life_date_t date)
{
    return date.year >= 1900U && date.year <= 9999U && date.month >= 1U &&
           date.month <= 12U && date.day >= 1U &&
           date.day <= life_countdown_days_in_month(date.year, date.month);
}

static int compare_dates(life_date_t left, life_date_t right)
{
    if (left.year != right.year) {
        return left.year < right.year ? -1 : 1;
    }
    if (left.month != right.month) {
        return left.month < right.month ? -1 : 1;
    }
    if (left.day != right.day) {
        return left.day < right.day ? -1 : 1;
    }
    return 0;
}

/* Proleptic Gregorian date to days since 1970-01-01. Validated years are
 * positive, so C's integer division has the same behavior as floor here. */
static int32_t days_from_civil(life_date_t date)
{
    int32_t year = date.year;
    year -= date.month <= 2U;
    int32_t era = year / 400;
    uint32_t year_of_era = (uint32_t)(year - era * 400);
    uint32_t shifted_month = (uint32_t)date.month +
                             (date.month > 2U ? (uint32_t)-3 : 9U);
    uint32_t day_of_year = (153U * shifted_month + 2U) / 5U + date.day - 1U;
    uint32_t day_of_era = year_of_era * 365U + year_of_era / 4U -
                          year_of_era / 100U + day_of_year;
    return era * 146097 + (int32_t)day_of_era - 719468;
}

bool life_countdown_add_years(life_date_t date, uint16_t years,
                              life_date_t *result)
{
    if (result == NULL || !life_countdown_date_is_valid(date) ||
        (uint32_t)date.year + years > 9999U) {
        return false;
    }

    *result = date;
    result->year = (uint16_t)(date.year + years);
    uint8_t days = life_countdown_days_in_month(result->year, result->month);
    if (result->day > days) {
        /* A Feb 29 birthday reaches the planning line on Feb 28 in a
         * non-leap target year. */
        result->day = days;
    }
    return true;
}

bool life_countdown_calculate(life_date_t birth_date, uint8_t planning_age,
                              life_date_t today,
                              life_countdown_metrics_t *metrics)
{
    if (metrics == NULL || !life_countdown_date_is_valid(birth_date) ||
        !life_countdown_date_is_valid(today) || planning_age < 1U ||
        planning_age > 120U) {
        return false;
    }

    life_date_t planning_date;
    if (!life_countdown_add_years(birth_date, planning_age, &planning_date)) {
        return false;
    }

    int32_t birth_days = days_from_civil(birth_date);
    int32_t today_days = days_from_civil(today);
    int32_t planning_days = days_from_civil(planning_date);
    int32_t lived_days = today_days - birth_days;
    int32_t remaining_days = planning_days - today_days;

    uint16_t age_years = 0U;
    uint32_t age_months = 0U;
    if (compare_dates(today, birth_date) >= 0) {
        int32_t years = (int32_t)today.year - birth_date.year;
        life_date_t anniversary;
        if (!life_countdown_add_years(birth_date, (uint16_t)years,
                                      &anniversary)) {
            return false;
        }
        if (compare_dates(today, anniversary) < 0) {
            --years;
        }
        age_years = (uint16_t)years;

        int32_t months = ((int32_t)today.year - birth_date.year) * 12 +
                         (int32_t)today.month - birth_date.month;
        uint8_t anniversary_day = birth_date.day;
        uint8_t current_month_days = life_countdown_days_in_month(today.year,
                                                                  today.month);
        if (anniversary_day > current_month_days) {
            anniversary_day = current_month_days;
        }
        if (today.day < anniversary_day) {
            --months;
        }
        age_months = months > 0 ? (uint32_t)months : 0U;
    }

    uint32_t total_days = (uint32_t)(planning_days - birth_days);
    uint32_t elapsed_for_progress = lived_days <= 0
                                        ? 0U
                                        : (uint32_t)lived_days;
    if (elapsed_for_progress > total_days) {
        elapsed_for_progress = total_days;
    }

    metrics->age_years = age_years;
    metrics->age_months = age_months;
    metrics->lived_days = lived_days > 0 ? (uint32_t)lived_days : 0U;
    metrics->lived_weeks = metrics->lived_days / 7U;
    metrics->remaining_days = remaining_days > 0
                                  ? (uint32_t)remaining_days
                                  : 0U;
    metrics->progress_percent = total_days == 0U
                                    ? 100U
                                    : (uint8_t)((elapsed_for_progress * 100U) /
                                                total_days);
    metrics->planning_date = planning_date;
    return true;
}
