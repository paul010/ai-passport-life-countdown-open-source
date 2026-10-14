#include "life_countdown_model.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(life_countdown_days_in_month(2000U, 2U) == 29U);
    assert(life_countdown_days_in_month(2100U, 2U) == 28U);
    assert(life_countdown_date_is_valid((life_date_t){2024U, 2U, 29U}));
    assert(!life_countdown_date_is_valid((life_date_t){2023U, 2U, 29U}));
    assert(life_countdown_days_in_month(2024U, 13U) == 0U);

    life_date_t target;
    assert(life_countdown_add_years((life_date_t){2020U, 2U, 29U}, 1U,
                                    &target));
    assert(target.year == 2021U && target.month == 2U && target.day == 28U);
    assert(!life_countdown_add_years((life_date_t){2020U, 2U, 29U}, 1U,
                                     NULL));

    life_countdown_metrics_t metrics;
    assert(life_countdown_calculate((life_date_t){2000U, 1U, 1U}, 75U,
                                    (life_date_t){2026U, 9U, 28U}, &metrics));
    assert(metrics.age_years == 26U);
    assert(metrics.age_months == 320U);
    assert(metrics.lived_days == 9767U);
    assert(metrics.lived_weeks == 1395U);
    assert(metrics.remaining_days == 17627U);
    assert(metrics.planning_date.year == 2075U &&
           metrics.planning_date.month == 1U &&
           metrics.planning_date.day == 1U);
    assert(metrics.progress_percent > 0U && metrics.progress_percent < 100U);

    assert(life_countdown_calculate((life_date_t){2000U, 2U, 29U}, 75U,
                                    (life_date_t){2021U, 2U, 28U}, &metrics));
    assert(metrics.age_years == 21U && metrics.age_months == 252U);
    assert(life_countdown_calculate((life_date_t){2000U, 1U, 31U}, 75U,
                                    (life_date_t){2000U, 2U, 29U}, &metrics));
    assert(metrics.age_years == 0U && metrics.age_months == 1U);

    assert(life_countdown_calculate((life_date_t){2024U, 1U, 1U}, 75U,
                                    (life_date_t){2024U, 1U, 1U}, &metrics));
    assert(metrics.lived_days == 0U && metrics.remaining_days == 27394U);
    assert(metrics.progress_percent == 0U);
    assert(life_countdown_calculate((life_date_t){2024U, 1U, 1U}, 75U,
                                    (life_date_t){2099U, 1U, 1U}, &metrics));
    assert(metrics.age_years == 75U && metrics.remaining_days == 0U);
    assert(metrics.progress_percent == 100U);

    assert(life_countdown_calculate((life_date_t){2027U, 5U, 3U}, 75U,
                                    (life_date_t){2026U, 5U, 3U}, &metrics));
    assert(metrics.age_years == 0U && metrics.age_months == 0U);
    assert(metrics.lived_days == 0U && metrics.lived_weeks == 0U);

    assert(!life_countdown_calculate((life_date_t){2000U, 2U, 30U}, 75U,
                                     (life_date_t){2026U, 9U, 28U}, &metrics));
    assert(!life_countdown_calculate((life_date_t){2000U, 1U, 1U}, 0U,
                                     (life_date_t){2026U, 9U, 28U}, &metrics));
    assert(!life_countdown_calculate((life_date_t){2000U, 1U, 1U}, 121U,
                                     (life_date_t){2026U, 9U, 28U}, &metrics));
    assert(!life_countdown_calculate((life_date_t){2000U, 1U, 1U}, 75U,
                                     (life_date_t){2026U, 9U, 28U}, NULL));

    puts("Life countdown calendar calculations: PASS");
    return 0;
}
