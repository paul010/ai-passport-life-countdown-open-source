#include "life_quest_nav.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    for (int page = 0; page < 7; ++page) {
        int n = life_quest_focus_count(page);
        for (int i = 0; i < n; ++i) {
            assert(life_quest_focus_next(page, i, 1) == (i + 1) % n);
            assert(life_quest_focus_next(page, i, -1) == (i + n - 1) % n);
        }
        if (page >= 4) {
            for (int i = 0; i < n; ++i) assert(life_quest_nav_target(page, i) == -1);
        } else {
            for (int i = 0; i < 4; ++i) assert(life_quest_nav_target(page, n - 4 + i) == i);
        }
    }
    assert(life_quest_focus_next(0, 0, -1) == 8); /* Settings is one UP away. */
    puts("Four-tab navigation and focus wrap: PASS");
}
