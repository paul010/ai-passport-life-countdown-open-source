#pragma once

#include <stdbool.h>
#include <stdint.h>

// Unsigned subtraction intentionally keeps this correct across a millisecond
// counter wrap. A zero timeout is treated as disabled.
bool idle_power_expired(uint32_t last_activity_ms, uint32_t now_ms,
                        uint32_t timeout_ms);
