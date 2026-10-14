#include "idle_power.h"

bool idle_power_expired(uint32_t last_activity_ms, uint32_t now_ms,
                        uint32_t timeout_ms)
{
    return timeout_ms != 0 && now_ms - last_activity_ms >= timeout_ms;
}
