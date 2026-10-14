#include "life_quest_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    life_quest_state_t s;
    life_quest_default(&s);
    assert(life_quest_valid(&s));
    assert(s.birth.year == 2000 && s.birth.month == 1 && s.birth.day == 1);
    int32_t day = life_quest_day((life_date_t){2026,9,30});
    assert(day - life_quest_day((life_date_t){2026,9,29}) == 1);
    assert(life_quest_day((life_date_t){2000,3,1}) -
           life_quest_day((life_date_t){2000,2,28}) == 2);
    uint8_t reward;
    assert(life_quest_complete(&s, day, 0, &reward) == LQ_OK && reward == 20);
    assert(life_quest_complete(&s, day, 0, &reward) == LQ_DUPLICATE && reward == 0);
    assert(life_quest_complete(&s, day, 1, &reward) == LQ_OK);
    assert(life_quest_tipo(&s, day) == LQ_OK && life_quest_hp(&s, day) == 0);
    assert(life_quest_xp(&s) == 40); /* Tipo never gives clear XP. */
    assert(life_quest_tipo(&s, day) == LQ_UNAVAILABLE);
    assert(life_quest_complete(&s, day, 2, &reward) == LQ_OK && reward == 60);
    assert(life_quest_xp(&s) == 100);
    assert(life_quest_undo(&s, day, &reward) == LQ_OK && reward == 60);
    assert(life_quest_xp(&s) == 40 && s.tipo_used);
    assert(life_quest_complete(&s, day, 2, &reward) == LQ_OK);
    assert(life_quest_complete(&s, day + 3, 1, &reward) == LQ_OK);
    assert(life_quest_xp(&s) == 120 && s.archived_clears == 1);
    life_quest_state_t copy = s;
    assert(life_quest_complete(&s, day, 0, &reward) == LQ_CLOCK_ROLLBACK);
    assert(!memcmp(&s, &copy, sizeof(s)));
    assert(life_quest_undo(&s, day, &reward) == LQ_CLOCK_ROLLBACK);
    s.partner = 1;
    assert(life_quest_tipo(&s, day + 3) == LQ_UNAVAILABLE);
    assert(life_quest_china_percent(44) == 53);
    assert(life_quest_age_group(44) == 105473092);
    assert(life_quest_china_percent(100) == 99);
    s.count = 2; s.order[0] = s.order[1] = 0;
    assert(!life_quest_valid(&s));
    puts("Life Quest model: PASS (XP, duplicate, undo, Tipo, rollover, rollback, population)");
}
