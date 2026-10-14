#include "life_quest_ui.h"
#include "profile_images.h"
#include "life_profile.h"
#include "quest_sprites.h"
#include <stdio.h>
#include <string.h>

lv_obj_t *life_quest_ui_label(lv_obj_t *p, const char *text, int x, int y,
                             const lv_font_t *font, uint32_t color, int width)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_line_space(l, 2, 0);
    lv_label_set_text(l, text);
    lv_obj_set_pos(l, x, y);
    if (width) {
        lv_obj_set_width(l, width);
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    }
    return l;
}

lv_obj_t *life_quest_ui_panel(lv_obj_t *p, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 3, 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(LQ_INK), 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_shadow_color(o, lv_color_hex(0x183456), 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    return o;
}

void life_quest_ui_focus(life_quest_ui_t *ui, lv_obj_t *o, bool focused)
{
    if (!focused) return;
    lv_obj_set_style_outline_color(o, lv_color_hex(LQ_YELLOW), 0);
    lv_obj_set_style_outline_width(o, 2, 0);
    lv_obj_set_style_outline_pad(o, 1, 0);
    ui->selected = o;
}

lv_obj_t *life_quest_ui_screen(life_quest_ui_t *ui, const char *title, int battery)
{
    lv_obj_t *s = lv_screen_active();
    lv_obj_clean(s); /* one live tree: no double allocation during navigation */
    memset(ui, 0, sizeof(*ui)); ui->screen = s;
    lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s, lv_color_hex(LQ_BLUE), 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_set_style_pad_all(s, 0, 0);
    ui->clock = life_quest_ui_label(s, "", 8, 5, &life_quest_font_11, 0xFFFFFF, 154);
    char text[40];
    if (battery >= 0) snprintf(text, sizeof(text), "电量 %d%%", battery);
    else snprintf(text, sizeof(text), "电量 --");
    life_quest_ui_label(s, text, 171, 5, &life_quest_font_11, 0xFFFFFF, 66);
    life_quest_ui_label(s, title, 9, 25, &life_quest_font_18, 0xFFFFFF, 150);
    life_quest_ui_label(s, "LIFE QUEST", 161, 31, &life_quest_font_11, 0xE0ECFF, 78);
    ui->notice = life_quest_ui_label(s, "", 9, 271, &life_quest_font_11, 0xFFFFFF, 220);
    lv_obj_add_flag(ui->notice, LV_OBJ_FLAG_HIDDEN);
    return s;
}

void life_quest_ui_tabs(life_quest_ui_t *ui, int active, int selected)
{
    static const char *names[] = {"营地", "任务", "人生", "设置"};
    lv_obj_t *bar = life_quest_ui_panel(ui->screen, 4, 290, 232, 27, LQ_MINT);
    lv_obj_set_style_radius(bar, 3, 0);
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *cell = life_quest_ui_panel(bar, 2 + i * 57, 1, 55, 22,
                                           i == active ? LQ_PAPER : LQ_MINT);
        lv_obj_set_style_border_width(cell, i == active ? 1 : 0, 0);
        life_quest_ui_focus(ui, cell, i == selected);
        lv_obj_t *l = life_quest_ui_label(cell, names[i], 0, 0,
                                         &life_quest_font_11, LQ_INK, 0);
        lv_obj_center(l);
    }
}

static lv_obj_t *image(lv_obj_t *p, const lv_image_dsc_t *src, int x, int y)
{
    lv_obj_t *o = lv_image_create(p);
    lv_image_set_src(o, src); lv_obj_set_pos(o, x, y); return o;
}

static void rect(lv_obj_t *p, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = life_quest_ui_panel(p, x, y, w, h, color);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
}

void life_quest_ui_home(life_quest_ui_t *ui, const life_quest_state_t *s,
                        life_date_t date, int battery, int choice)
{
    lv_obj_t *scr = life_quest_ui_screen(ui, "人生冒险", battery);
    int32_t day = life_quest_day(date);
    uint32_t xp = life_quest_xp(s);
    life_countdown_metrics_t m = {0};
    (void)life_countdown_calculate(s->birth, s->planning_age, date, &m);
    lv_obj_t *card = life_quest_ui_panel(scr, 8, 49, 224, 70, LQ_PAPER);
    image(card, &quest_card_avatar_image, 5, 5);
    life_quest_ui_label(card, s->name ? LIFE_PROFILE_ALTERNATE_NAME : LIFE_PROFILE_NAME, 51, 3, &life_quest_font_18, LQ_INK, 110);
    life_quest_ui_label(card, LIFE_PROFILE_ROLE, 51, 23, &life_quest_font_11, LQ_PURPLE, 0);
    char text[80];
    snprintf(text, sizeof(text), "%04u.%02u.%02u", s->birth.year, s->birth.month, s->birth.day);
    life_quest_ui_label(card, text, 51, 35, &life_quest_font_11, LQ_MUTED, 0);
    lv_obj_t *chip = life_quest_ui_panel(card, 175, 5, 41, 21, 0xEEE3FF);
    lv_obj_set_style_border_color(chip, lv_color_hex(0xB190E5), 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    snprintf(text, sizeof(text), "Lv.%lu", (unsigned long)(1 + xp / 100));
    lv_obj_t *lv = life_quest_ui_label(chip, text, 0, 0, &life_quest_font_11, LQ_PURPLE, 0);
    lv_obj_center(lv);
    rect(card, 51, 48, 163, 4, 0xDDDCEC);
    if (xp % 100) rect(card, 51, 48, (int)(163 * (xp % 100) / 100), 4, 0xAC8CE3);
    snprintf(text, sizeof(text), "成长经验 · %lu/100 XP", (unsigned long)(xp % 100));
    life_quest_ui_label(card, text, 51, 53, &life_quest_font_11, LQ_MUTED, 165);

    lv_obj_t *arena = life_quest_ui_panel(scr, 8, 126, 224, 81, 0xD9E9FF);
    rect(arena, 0, 48, 220, 30, 0x9CCBA6);
    rect(arena, 0, 65, 220, 13, 0x86B66B);
    static const char *enemies[] = {"拖延怪", "分心怪", "刷屏怪"};
    life_quest_ui_label(arena, life_quest_count(s, day) == 3 ? "今日通关" : enemies[(unsigned)day % 3],
                        6, 5, &life_quest_font_14, LQ_INK, 0);
    life_quest_ui_label(arena, "今日小怪", 69, 8, &life_quest_font_11, LQ_MUTED, 0);
    unsigned hp = life_quest_hp(s, day);
    for (int i = 0; i < 3; ++i) rect(arena, 182 + i * 11, 8, 7, 6,
                                    (unsigned)i < hp ? 0xED7785 : 0xC6D0E5);
    for (int i = 0; i < 2; ++i) {
        lv_obj_t *button = life_quest_ui_panel(arena, 8 + i * 37, 29, 31, 32,
                                              i ? 0xF8DECF : LQ_PAPER);
        lv_obj_set_style_border_width(button, 1, 0);
        lv_obj_set_style_border_color(button, lv_color_hex(s->partner == i ? LQ_PURPLE : LQ_MUTED), 0);
        life_quest_ui_focus(ui, button, choice == i + 2);
        lv_obj_t *icon = image(button, i ? &quest_reed_image : &quest_tipo_image, 2, 2);
        if (s->partner == i) ui->buddy = icon;
    }
    bool used = s->latest_day == day && s->tipo_used;
    lv_obj_t *tipo = life_quest_ui_panel(arena, 87, 30, 45, 34,
                                        used || s->partner || hp == 0 ? 0xDDDCEC : 0xEEE0FF);
    lv_obj_set_style_border_width(tipo, 1, 0);
    life_quest_ui_focus(ui, tipo, choice == 1);
    life_quest_ui_label(tipo, "Tipo", 10, 2, &life_quest_font_11, LQ_PURPLE, 0);
    life_quest_ui_label(tipo, hp == 0 ? "休息" : used ? "已用" : "大招", 10, 16,
                        &life_quest_font_11, LQ_PURPLE, 0);
    ui->monster = image(arena, &quest_monster_images[(unsigned)day % 3], 157, 25);
    if (hp == 0) lv_obj_set_style_opa(ui->monster, LV_OPA_40, 0);
    life_quest_ui_label(arena, s->partner ? "REED · 切换伙伴" : "TIPO · 切换伙伴",
                        6, 64, &life_quest_font_11, 0x426442, 0);

    lv_obj_t *cta = life_quest_ui_panel(scr, 8, 216, 224, 29, LQ_YELLOW);
    life_quest_ui_focus(ui, cta, choice == 0);
    life_quest_ui_label(cta, "开始今日冒险", 29, 5, &life_quest_font_14, LQ_INK, 0);
    snprintf(text, sizeof(text), "%u/3 完成", life_quest_count(s, day));
    life_quest_ui_label(cta, text, 154, 8, &life_quest_font_11, 0x756644, 0);

    lv_obj_t *peek = life_quest_ui_panel(scr, 8, 251, 224, 33, LQ_BLUE);
    lv_obj_set_style_border_width(peek, 0, 0);
    life_quest_ui_focus(ui, peek, choice == 4);
    life_quest_ui_label(peek, "生命词元 LT", 1, 0, &life_quest_font_11, 0xFFFFFF, 0);
    uint32_t lt = m.lived_days * 1440U;
    snprintf(text, sizeof(text), "%lu.%02luM LT", (unsigned long)(lt / 1000000U),
              (unsigned long)(lt % 1000000U / 10000U));
    life_quest_ui_label(peek, text, 1, 15, &life_quest_font_14, 0xFFFFFF, 0);
    snprintf(text, sizeof(text), "人生电量 · %u岁", s->planning_age);
    life_quest_ui_label(peek, text, 116, 0, &life_quest_font_11, 0xFFFFFF, 0);
    int32_t span = life_quest_day(m.planning_date) - life_quest_day(s->birth);
    unsigned tenth = span > 0 ? (unsigned)((uint64_t)m.remaining_days * 1000U / span) : 0;
    if (tenth > 1000) tenth = 1000;
    snprintf(text, sizeof(text), "%u.%u%%", tenth / 10, tenth % 10);
    life_quest_ui_label(peek, text, 116, 15, &life_quest_font_14, 0xFFFFFF, 0);
    life_quest_ui_tabs(ui, 0, choice >= 5 ? choice - 5 : -1);
}
