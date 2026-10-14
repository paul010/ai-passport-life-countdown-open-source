#pragma once
#include "bsp_button.h"
#include <stdbool.h>
#include <stdint.h>
bool life_quest_app_init(int battery, bool storage_ok, bool retained_clock);
void life_quest_app_enter(void);
void life_quest_app_key(bsp_btn_t key, bsp_btn_ev_t event, uint32_t request);
bool life_quest_app_can_sleep(void);
/* Local USB diagnostics: bounded, explicit commands only. No network. */
void life_quest_app_command(const char *command);
