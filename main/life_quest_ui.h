#pragma once
#include "lvgl.h"
#include "life_quest_model.h"
#include "life_quest_nav.h"

/* Pure presentation shared by the device and native LVGL render checks. */
#define LQ_BLUE 0x2675DC
#define LQ_INK 0x263047
#define LQ_PAPER 0xFFFDF5
#define LQ_YELLOW 0xFFE37B
#define LQ_PURPLE 0x7658B3
#define LQ_MINT 0xD8E7A3
#define LQ_MUTED 0x7885A2

LV_FONT_DECLARE(life_quest_font_11);
LV_FONT_DECLARE(life_quest_font_14);
LV_FONT_DECLARE(life_quest_font_18);

typedef struct {
    lv_obj_t *screen, *clock, *notice, *selected, *monster, *buddy;
} life_quest_ui_t;

lv_obj_t *life_quest_ui_screen(life_quest_ui_t *ui, const char *title, int battery);
lv_obj_t *life_quest_ui_label(lv_obj_t *parent, const char *text, int x, int y,
                             const lv_font_t *font, uint32_t color, int width);
lv_obj_t *life_quest_ui_panel(lv_obj_t *parent, int x, int y, int w, int h,
                             uint32_t color);
void life_quest_ui_focus(life_quest_ui_t *ui, lv_obj_t *obj, bool focused);
void life_quest_ui_tabs(life_quest_ui_t *ui, int active, int selected);
void life_quest_ui_home(life_quest_ui_t *ui, const life_quest_state_t *state,
                        life_date_t date, int battery, int choice);
