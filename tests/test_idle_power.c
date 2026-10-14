#include "idle_power.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    assert(!idle_power_expired(1000, 2000, 0));
    assert(!idle_power_expired(1000, 300999, 300000));
    assert(idle_power_expired(1000, 301000, 300000));
    assert(idle_power_expired(UINT32_MAX - 99, 100, 200));
    puts("Idle power timing tests: PASS");
    return 0;
}
