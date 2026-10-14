#include "life_quest_nav.h"

int life_quest_focus_count(int page)
{
    static const int counts[] = {9, 8, 9, 9, 5, 5, 2};
    return page >= 0 && page < 7 ? counts[page] : 1;
}

int life_quest_nav_target(int page, int choice)
{
    int start = page == 1 ? 4 : (page >= 0 && page <= 3 ? 5 : -1);
    return start >= 0 && choice >= start && choice < start + 4 ? choice - start : -1;
}

int life_quest_focus_next(int page, int choice, int delta)
{
    int count = life_quest_focus_count(page);
    int next = (choice + delta) % count;
    return next < 0 ? next + count : next;
}
