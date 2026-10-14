#pragma once

#include "bsp_button.h"
#include <stdbool.h>

bool life_countdown_app_init(int battery_percent);
void life_countdown_app_enter(void);
void life_countdown_app_exit(void);
bool life_countdown_app_key(bsp_btn_t button, bsp_btn_ev_t event);
