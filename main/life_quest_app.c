#include "life_quest_app.h"
#include "life_quest_model.h"
#include "life_profile.h"
#include "profile_images.h"
#include "ui_pixel.h"
#include "life_quest_ui.h"
#include "fap_console.h"
#include "bsp_audio.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

typedef enum { HOME, TASKS, LIFE, SETTINGS, PROFILE, CLOCK, CLEAR } page_t;
typedef struct { life_quest_state_t state; uint8_t save, tone; } job_t;
typedef struct { bool ok; } result_t;
static const char *TAG = "life_quest";
static life_quest_state_t s_state, s_draft, s_pending_state;
static QueueHandle_t s_jobs, s_results;
static bool s_storage, s_pending, s_clock_checked;
static atomic_bool s_audio;
static atomic_uint s_tone_count;
static page_t s_page;
static int s_choice, s_life_page, s_battery;
static struct tm s_clock_draft;
static uint8_t s_after_tone;
static uint32_t s_last_tick;
static uint32_t s_input_count, s_input_request;
static int s_input_key, s_input_event, s_input_page, s_input_choice;
static uint32_t s_physical_count;
static int s_physical_key, s_physical_event, s_physical_page, s_physical_choice;
static int32_t s_render_day;
static life_quest_ui_t s_ui;
#define s_clock_label s_ui.clock
#define s_notice s_ui.notice
#define s_monster s_ui.monster
#define s_selected s_ui.selected
static const char *s_message = "上下选择 · OK 打开";
static void render(void);
static void toast(const char *message);

static life_date_t today(void)
{
    time_t now = time(NULL);
    struct tm local = {0};
    localtime_r(&now, &local);
    return (life_date_t){local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
}
static int32_t day_now(void) { return life_quest_day(today()); }
static bool metrics(life_countdown_metrics_t *m)
{
    return life_countdown_calculate(s_state.birth, s_state.planning_age, today(), m);
}
static const char *name(void) { return s_state.name ? LIFE_PROFILE_ALTERNATE_NAME : LIFE_PROFILE_NAME; }

/* Short enveloped triangle tones; one worker serializes codec writes and NVS
 * commits. There is never a Flash write during an active audio feed. */
static void note(unsigned hz, unsigned duration)
{
    int16_t pcm[128];
    unsigned total = duration * 16U, period = 16000U / hz;
    for (unsigned off = 0; off < total; off += 128) {
        unsigned n = total - off > 128 ? 128 : total - off;
        for (unsigned i = 0; i < n; ++i) {
            unsigned pos = off + i, phase = pos % period;
            int wave = phase < period / 2 ? (int)(phase * 2400U / (period / 2)) - 1200
                                         : 1200 - (int)((phase - period / 2) * 2400U / (period / 2));
            unsigned fade = pos < 160 ? pos : (total - pos < 160 ? total - pos : 160);
            pcm[i] = (int16_t)(wave * (int)fade / 160);
        }
        if (bsp_audio_write(pcm, n * sizeof(int16_t)) != ESP_OK) break;
    }
}
static void sound(uint8_t tone)
{
    if (tone == 1) note(640, 45);
    if (tone == 2) { note(650, 75); note(870, 90); }
    if (tone == 3) { note(650, 85); note(870, 85); note(1100, 160); }
    if (tone == 4) { note(700, 70); note(450, 100); }
    if (tone == 5) { note(430, 70); note(800, 140); }
}
static bool commit(const life_quest_state_t *state)
{
    if (!s_storage) return false;
    nvs_handle_t h;
    esp_err_t r = nvs_open("lifequest", NVS_READWRITE, &h);
    if (r != ESP_OK) return false;
    r = nvs_set_blob(h, "state_v1", state, sizeof(*state));
    if (r == ESP_OK) r = nvs_commit(h);
    nvs_close(h);
    return r == ESP_OK;
}
static void worker(void *arg)
{
    (void)arg;
    bool audio = bsp_audio_init() == ESP_OK;
    if (audio) audio = bsp_audio_set_format(16000, 16, 1) == ESP_OK;
    if (audio) bsp_audio_set_volume(28);
    atomic_store(&s_audio, audio);
    ESP_LOGI(TAG, "Short-tone audio %s", audio ? "ready" : "unavailable; visual feedback remains");
    job_t job;
    for (;;) {
        if (xQueueReceive(s_jobs, &job, portMAX_DELAY) != pdTRUE) continue;
        bool saved = true;
        if (job.save) {
            saved = commit(&job.state);
            result_t result = {saved};
            xQueueOverwrite(s_results, &result);
        }
        if (audio && saved && job.state.sound) {
            sound(job.tone);
            if (job.tone) atomic_fetch_add(&s_tone_count, 1);
        }
    }
}
static void tap(void)
{
    if (!s_state.sound || s_pending) return;
    job_t j = {.state = s_state, .tone = 1};
    (void)xQueueSend(s_jobs, &j, 0);
}
static void save(const life_quest_state_t *state, uint8_t tone)
{
    if (s_pending) return;
    job_t j = {.state = *state, .save = 1, .tone = tone};
    if (tone == 2 && life_quest_xp(state) / 100 > life_quest_xp(&s_state) / 100)
        j.tone = 3;
    if (!s_storage || xQueueSend(s_jobs, &j, 0) != pdTRUE) {
        toast("保存失败，请稍后再试");
        return;
    }
    s_pending_state = *state;
    s_pending = true;
    s_after_tone = tone;
    s_message = "正在保存，请稍候";
    render();
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y,
                       const lv_font_t *font, uint32_t color, int width)
{
    return life_quest_ui_label(parent, text, x, y, font, color, width);
}
#define SMALL(p,t,x,y,c,w) label(p,t,x,y,&life_quest_font_11,c,w)
#define BODY(p,t,x,y,c,w) label(p,t,x,y,&life_quest_font_14,c,w)
#define BIG(p,t,x,y,c,w) label(p,t,x,y,&life_quest_font_18,c,w)
static lv_obj_t *screen(const char *title)
{
    lv_obj_t *scr = life_quest_ui_screen(&s_ui, title, s_battery);
    SMALL(scr, s_page == PROFILE || s_page == CLOCK
          ? "上增下减 · OK 下一项 · 长按取消"
          : "上下选择 · OK 确认 · 长按回营地", 13, 275, 0xFFFFFF, 220);
    life_quest_ui_tabs(&s_ui, s_page <= SETTINGS ? s_page : SETTINGS,
                       life_quest_nav_target(s_page, s_choice));
    lv_label_set_text(s_notice, s_message);
    s_render_day = day_now();
    return scr;
}
static void row(lv_obj_t *scr, int index, int y, const char *text)
{
    lv_obj_t *p = life_quest_ui_panel(scr, 9, y, 222, 33,
                                      s_choice == index ? LQ_YELLOW : LQ_PAPER);
    life_quest_ui_focus(&s_ui, p, s_choice == index);
    BODY(p, text, 6, 6, LQ_INK, 206);
}
static void home(void)
{
    life_quest_ui_home(&s_ui, &s_state, today(), s_battery, s_choice);
    s_render_day = day_now();
    if (!s_clock_checked) {
        lv_label_set_text(s_clock_label, "日期待核对 · 去设置校准");
    }
    if (!s_storage) lv_label_set_text(s_clock_label, "存档异常 · 原记录已保留");
}
static void tasks(void)
{
    lv_obj_t *scr = screen("今日任务");
    static const char *titles[] = {"学习 20 分钟", "活动或休息 15 分钟", "做一个小作品", "撤销最近一次打卡"};
    SMALL(scr, "先完成现实行动，再确认打卡", 10, 53, 0xFFFFFF, 220);
    for (int i = 0; i < 4; ++i) {
        char text[96];
        bool done = i < 3 && life_quest_done(&s_state, day_now(), i);
        snprintf(text, sizeof(text), "%s%s", done ? "[完成] " : "", titles[i]);
        row(scr, i, 77 + i * 43, text);
    }
    SMALL(scr, "每项 20 XP · 全部完成再加 40 XP\n漏打卡不扣分，成长一直保留", 10, 244, 0xFFFFFF, 220);
}
static void life(void)
{
    lv_obj_t *scr = screen("人生档案");
    life_countdown_metrics_t m;
    if (!metrics(&m)) { BIG(scr, "请核对日期与生日", 10, 85, 0xFFFFFF, 218); return; }
    static const char *sections[] = {"数字", "词元", "中国", "世界", "里程碑"};
    for (int i = 0; i < 5; ++i) {
        lv_obj_t *tab = life_quest_ui_panel(scr, 9 + i * 44, 55, 42, 24,
                                           s_life_page == i ? LQ_YELLOW : LQ_PAPER);
        life_quest_ui_focus(&s_ui, tab, s_choice == i);
        lv_obj_t *l = SMALL(tab, sections[i], 0, 0, LQ_INK, 0); lv_obj_center(l);
    }
    lv_obj_t *p = life_quest_ui_panel(scr, 9, 89, 222, 180, LQ_PAPER);
    char text[480];
    if (s_life_page == 0) {
        snprintf(text, sizeof(text), "已经度过 %u 年 · 计划 %u 岁\n\n%lu 月       %lu 周\n%lu 天\n%lu 小时\n\n距计划还有 %lu 天\n个人计划，不是寿命预测",
            m.age_years, s_state.planning_age, (unsigned long)m.age_months,
            (unsigned long)m.lived_weeks, (unsigned long)m.lived_days,
            (unsigned long)(m.lived_days * 24U), (unsigned long)m.remaining_days);
    } else if (s_life_page == 1) {
        snprintf(text, sizeof(text), "生命词元 LT\n\n已经走过：%lu LT\n距计划：%lu LT\n\n1 分钟 = 1 LT\n按已经过完的日历天换算\n不是模型词元，也不是钱",
            (unsigned long)(m.lived_days * 1440U), (unsigned long)(m.remaining_days * 1440U));
    } else if (s_life_page == 2) {
        uint16_t group = m.age_years / 5U * 5U;
        snprintf(text, sizeof(text), "中国 · 年龄坐标\n\n年龄高于约 %u%%%s 的人口\n同年龄段：%u-%u 岁\n约 %lu 人\n\n2026 年预测 · 五岁组估算\n不是个人排行榜",
            life_quest_china_percent(m.age_years), m.age_years >= 100 ? "+" : "",
            group, group + 4, (unsigned long)life_quest_age_group(m.age_years));
    } else if (s_life_page == 3) {
        int delta = (int)m.age_years * 10 - 311;
        snprintf(text, sizeof(text), "世界 · 人口星球\n\n全球中位年龄：31.1 岁\n你的年龄：%u 岁\n相差：%s%d.%d 岁\n\n2026 年预测人口 83.01 亿\n不编造全球个人名次\n来源：UN WPP 转录表",
            m.age_years, delta < 0 ? "-" : "+", abs(delta) / 10, abs(delta) % 10);
    } else {
        life_date_t d = today(), birthday;
        unsigned years = d.year >= s_state.birth.year ? d.year - s_state.birth.year : 0;
        if (!life_countdown_add_years(s_state.birth, years, &birthday)) birthday = d;
        if (life_quest_day(birthday) < day_now())
            (void)life_countdown_add_years(s_state.birth, years + 1, &birthday);
        uint32_t next = (m.lived_days / 1000U + 1U) * 1000U;
        snprintf(text, sizeof(text), "下一个里程碑\n\n距生日：%ld 天\n人生第 %lu 天\n再过 %lu 天就到达\n\n累计通关：%lu 天\n按自己的节奏，慢慢成长",
            (long)(life_quest_day(birthday) - day_now()), (unsigned long)next,
            (unsigned long)(next - m.lived_days),
            (unsigned long)(s_state.archived_clears + (s_state.count == 3)));
    }
    BODY(p, text, 8, 6, LQ_INK, 202);
}
static void settings(void)
{
    lv_obj_t *scr = screen("设置");
    char sounds[40], motion[40];
    snprintf(sounds, sizeof(sounds), "音效：%s", s_state.sound ? "开启" : "静音");
    snprintf(motion, sizeof(motion), "动画：%s", s_state.motion ? "开启" : "减少动态");
    const char *items[] = {"编辑资料、生日与计划", "校准设备日期与时间", sounds, motion, "重置游戏进度"};
    for (int i = 0; i < 5; ++i) row(scr, i, 57 + i * 41, items[i]);
}
static void profile(void)
{
    lv_obj_t *scr = screen("编辑资料");
    char text[60];
    snprintf(text, sizeof(text), "名字：%s", s_draft.name ? LIFE_PROFILE_ALTERNATE_NAME : LIFE_PROFILE_NAME); row(scr, 0, 57, text);
    snprintf(text, sizeof(text), "出生年份：%u", s_draft.birth.year); row(scr, 1, 98, text);
    snprintf(text, sizeof(text), "出生月份：%u", s_draft.birth.month); row(scr, 2, 139, text);
    snprintf(text, sizeof(text), "出生日期：%u", s_draft.birth.day); row(scr, 3, 180, text);
    snprintf(text, sizeof(text), "计划年龄：%u 岁", s_draft.planning_age); row(scr, 4, 221, text);
    SMALL(scr, "最后一项按 OK 保存 · 不改变经验", 10, 259, 0xFFFFFF, 220);
}
static void clock_page(void)
{
    lv_obj_t *scr = screen("校准日期");
    char text[48];
    int values[] = {s_clock_draft.tm_year + 1900, s_clock_draft.tm_mon + 1,
                   s_clock_draft.tm_mday, s_clock_draft.tm_hour, s_clock_draft.tm_min};
    const char *fields[] = {"年份", "月份", "日期", "小时（北京时间）", "分钟"};
    for (int i = 0; i < 5; ++i) {
        snprintf(text, sizeof(text), "%s: %d", fields[i], values[i]);
        row(scr, i, 57 + i * 41, text);
    }
    SMALL(scr, "确认真实日期后才能领取任务经验", 10, 259, 0xFFFFFF, 220);
}
static void clear_page(void)
{
    lv_obj_t *scr = screen("重置游戏？");
    BIG(scr, "确定清空经验与打卡？", 14, 78, 0xFFFFFF, 212);
    BODY(scr, "生日、名字、计划年龄\n和设备身份都会保留。\n此操作不能撤销。", 14, 132, 0xFFFFFF, 208);
    row(scr, 0, 192, "取消，保留进度");
    row(scr, 1, 234, "确认，仅清空游戏");
}
static void update_clock(void)
{
    if (!s_clock_label) return;
    if (s_page == HOME && !s_clock_checked) return;
    time_t now = time(NULL); struct tm local;
    localtime_r(&now, &local);
    char text[44];
    strftime(text, sizeof(text), "%Y.%m.%d %H:%M", &local);
    if (day_now() < s_state.latest_day) snprintf(text, sizeof(text), "日期回退，请校准");
    lv_label_set_text(s_clock_label, text);
}
static void render(void)
{
    switch (s_page) {
    case HOME: home(); break;
    case TASKS: tasks(); break;
    case LIFE: life(); break;
    case SETTINGS: settings(); break;
    case PROFILE: profile(); break;
    case CLOCK: clock_page(); break;
    case CLEAR: clear_page(); break;
    }
    update_clock();
    if (s_pending) {
        lv_obj_t *scr = lv_screen_active();
        lv_obj_t *bar = life_quest_ui_panel(scr, 16, 249, 208, 28, LQ_YELLOW);
        BODY(bar, "正在保存，请稍候", 8, 4, LQ_INK, 190);
    }
}
static void toast(const char *message)
{
    s_message = message;
    render();
    lv_obj_remove_flag(s_notice, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(s_notice, 10, 260);
    lv_obj_set_height(s_notice, 28);
    lv_label_set_long_mode(s_notice, LV_LABEL_LONG_WRAP);
    lv_obj_move_foreground(s_notice);
    lv_obj_set_style_bg_color(s_notice, lv_color_hex(LQ_INK), 0);
    lv_obj_set_style_bg_opa(s_notice, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(s_notice, &life_quest_font_11, 0);
}
static void tick(lv_timer_t *timer)
{
    (void)timer;
    result_t result;
    if (s_pending && xQueueReceive(s_results, &result, 0) == pdTRUE) {
        s_pending = false;
        if (result.ok) {
            uint32_t old_xp = life_quest_xp(&s_state), new_xp = life_quest_xp(&s_pending_state);
            bool leveled = new_xp / 100 > old_xp / 100;
            bool reset = s_state.latest_day && !s_pending_state.latest_day;
            s_state = s_pending_state;
            toast(reset ? "游戏已重置，个人资料保留" :
                  new_xp < old_xp ? "已撤销，经验已退回" :
                  new_xp > old_xp && s_state.count == 3 ?
                    (leveled ? "+60 XP！今日通关，升级啦" : "+60 XP！今日通关") :
                  new_xp > old_xp ? (leveled ? "+20 XP！升级啦" : "+20 XP！行动获得回响") :
                  s_after_tone == 5 ? "Tipo 出招！清除一格干扰" : "已保存到这台设备");
            if (s_state.motion && s_monster) ui_pixel_mascot_jump(s_monster);
            if (s_state.motion && s_selected) ui_pixel_mascot_jump(s_selected);
        } else toast("保存失败，本次未记录");
    }
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (now - s_last_tick < 1000) return;
    s_last_tick = now;
    if (day_now() != s_render_day && s_page != PROFILE && s_page != CLOCK && !s_pending)
        render();
    else update_clock();
}
static void enter_clock(void)
{
    time_t now = time(NULL); localtime_r(&now, &s_clock_draft);
    s_clock_draft.tm_sec = 0;
    s_page = CLOCK; s_choice = 0; render();
}
static bool set_clock(struct tm value)
{
    life_date_t d = {value.tm_year + 1900, value.tm_mon + 1, value.tm_mday};
    if (value.tm_year < 124 || value.tm_year > 199 || value.tm_mon < 0 || value.tm_mon > 11 ||
        !life_countdown_date_is_valid(d) || d.year < 2024 || d.year > 2099 ||
        value.tm_hour < 0 || value.tm_hour > 23 || value.tm_min < 0 || value.tm_min > 59)
        return false;
    value.tm_sec = 0; value.tm_isdst = -1;
    time_t epoch = mktime(&value);
    struct timeval tv = {.tv_sec = epoch};
    if (epoch < 0 || settimeofday(&tv, NULL) != 0) return false;
    s_clock_checked = true; return true;
}
static int cycle(int value, int delta, int min, int max)
{
    int next = value + delta;
    return next > max ? min : next < min ? max : next;
}
static void edit(int delta)
{
    if (s_page == PROFILE) {
        if (s_choice == 0) s_draft.name ^= 1;
        if (s_choice == 1) s_draft.birth.year = cycle(s_draft.birth.year, delta, 1900, 2099);
        if (s_choice == 2) s_draft.birth.month = cycle(s_draft.birth.month, delta, 1, 12);
        if (s_choice == 3) s_draft.birth.day = cycle(s_draft.birth.day, delta, 1,
            life_countdown_days_in_month(s_draft.birth.year, s_draft.birth.month));
        if (s_choice == 4) s_draft.planning_age = cycle(s_draft.planning_age, delta, 50, 120);
        uint8_t last = life_countdown_days_in_month(s_draft.birth.year, s_draft.birth.month);
        if (s_draft.birth.day > last) s_draft.birth.day = last;
    } else {
        if (s_choice == 0) s_clock_draft.tm_year = cycle(s_clock_draft.tm_year, delta, 124, 199);
        if (s_choice == 1) s_clock_draft.tm_mon = cycle(s_clock_draft.tm_mon, delta, 0, 11);
        if (s_choice == 2) s_clock_draft.tm_mday = cycle(s_clock_draft.tm_mday, delta, 1,
            life_countdown_days_in_month(s_clock_draft.tm_year + 1900, s_clock_draft.tm_mon + 1));
        if (s_choice == 3) s_clock_draft.tm_hour = cycle(s_clock_draft.tm_hour, delta, 0, 23);
        if (s_choice == 4) s_clock_draft.tm_min = cycle(s_clock_draft.tm_min, delta, 0, 59);
        unsigned last = life_countdown_days_in_month(s_clock_draft.tm_year + 1900, s_clock_draft.tm_mon + 1);
        if ((unsigned)s_clock_draft.tm_mday > last) s_clock_draft.tm_mday = last;
    }
    render();
}
static bool ready_for_reward(void)
{
    if (!s_clock_checked) { enter_clock(); toast("请先确认真实日期，再打卡"); return false; }
    if (day_now() < s_state.latest_day) { toast("日期回退，请去设置校准"); return false; }
    if (life_quest_day(s_state.birth) > day_now()) { toast("生日在未来，请修改资料"); return false; }
    return true;
}
void life_quest_app_key(bsp_btn_t key, bsp_btn_ev_t ev, uint32_t request)
{
    ++s_input_count; s_input_request = request;
    s_input_key = key; s_input_event = ev; s_input_page = s_page; s_input_choice = s_choice;
    if (!request) {
        ++s_physical_count;
        s_physical_key = key; s_physical_event = ev;
        s_physical_page = s_page; s_physical_choice = s_choice;
    }
    if (ev == BSP_BTN_LONG && key == BSP_BTN_OK) {
        if (!s_pending) { s_page = HOME; s_choice = 0; s_message = "上下选择 · OK 打开"; render(); }
        return;
    }
    if (ev != BSP_BTN_CLICK || s_pending) return;
    if (key == BSP_BTN_UP || key == BSP_BTN_DOWN) {
        tap();
        int delta = key == BSP_BTN_UP ? -1 : 1;
        if (s_page == PROFILE || s_page == CLOCK) { edit(-delta); return; }
        s_choice = life_quest_focus_next(s_page, s_choice, delta);
        if (s_page == LIFE && s_choice < 5) s_life_page = s_choice;
        render(); return;
    }
    if (key != BSP_BTN_OK) return;
    int tab = life_quest_nav_target(s_page, s_choice);
    if (tab >= 0) {
        tap(); s_page = (page_t)tab; s_choice = 0; s_life_page = 0; render(); return;
    }
    if (s_page == HOME) {
        if (s_choice == 0) { s_page = TASKS; s_choice = 0; render(); }
        else if (s_choice == 1) {
            if (!ready_for_reward()) return;
            life_quest_state_t next = s_state;
            if (life_quest_tipo(&next, day_now()) == LQ_OK) save(&next, 5);
            else toast(s_state.partner ? "先选择 TIPO 伙伴" : "大招已用，或小怪已击败");
        } else if (s_choice == 2 || s_choice == 3) {
            life_quest_state_t next = s_state; next.partner = s_choice == 3;
            if (next.partner != s_state.partner) save(&next, 1);
            else toast("这个伙伴已经在陪你啦");
        } else if (s_choice == 4) { s_page = LIFE; s_life_page = s_choice = 1; render(); }
    } else if (s_page == TASKS) {
        if (!ready_for_reward()) return;
        life_quest_state_t next = s_state; uint8_t value;
        life_quest_result_t result = s_choice < 3
            ? life_quest_complete(&next, day_now(), s_choice, &value)
            : life_quest_undo(&next, day_now(), &value);
        if (result == LQ_OK) save(&next, s_choice == 3 ? 4 : value == 60 ? 3 : 2);
        else toast(result == LQ_DUPLICATE ? "今天已完成，不重复加经验" : "今天没有可撤销的打卡");
    } else if (s_page == LIFE) {
        tap(); /* section focus is already shown; long OK always returns home */
    } else if (s_page == SETTINGS) {
        if (s_choice == 0) { s_draft = s_state; s_page = PROFILE; s_choice = 0; render(); }
        else if (s_choice == 1) enter_clock();
        else if (s_choice < 4) {
            life_quest_state_t next = s_state;
            if (s_choice == 2) next.sound ^= 1; else next.motion ^= 1;
            save(&next, 1);
        } else { s_page = CLEAR; s_choice = 0; render(); }
    } else if (s_page == PROFILE || s_page == CLOCK) {
        if (s_choice < 4) { ++s_choice; render(); return; }
        if (s_page == PROFILE) {
            if (life_quest_day(s_draft.birth) > day_now()) { toast("生日不能在未来"); return; }
            s_page = HOME; s_choice = 0; save(&s_draft, 1);
        } else {
            if (set_clock(s_clock_draft)) { s_page = HOME; s_choice = 0; toast("日期已确认，离线可用"); }
            else toast("日期无效，请重新校准");
        }
    } else if (s_page == CLEAR) {
        s_page = SETTINGS;
        if (s_choice == 1) {
            life_quest_state_t next = s_state;
            next.count = next.tipo_used = 0; next.archived_xp = next.archived_clears = 0;
            next.latest_day = 0; memset(next.order, 0, sizeof(next.order));
            s_choice = 4; save(&next, 4);
        } else { s_choice = 4; render(); }
    }
}
bool life_quest_app_init(int battery, bool storage_ok, bool retained_clock)
{
    s_battery = battery; s_storage = storage_ok; s_clock_checked = retained_clock;
    life_quest_default(&s_state);
    if (storage_ok) {
        nvs_handle_t h;
        if (nvs_open("lifequest", NVS_READONLY, &h) == ESP_OK) {
            life_quest_state_t loaded; size_t size = sizeof(loaded);
            esp_err_t read = nvs_get_blob(h, "state_v1", &loaded, &size);
            if (read == ESP_OK && size == sizeof(loaded) && life_quest_valid(&loaded)) s_state = loaded;
            else if (read != ESP_ERR_NVS_NOT_FOUND) {
                s_storage = false; /* Preserve corrupt/incompatible records. */
                ESP_LOGE(TAG, "Game record invalid; preserved without overwrite");
            }
            nvs_close(h);
        } else if (nvs_open("lifeclock", NVS_READONLY, &h) == ESP_OK) {
            struct { uint32_t magic; life_date_t birth; uint8_t plan, reserved[3]; } old;
            size_t size = sizeof(old);
            if (nvs_get_blob(h, "profile", &old, &size) == ESP_OK && size == sizeof(old) &&
                old.magic == 0x4c494645U && life_countdown_date_is_valid(old.birth) &&
                old.birth.year <= 2099 && old.plan >= 50 && old.plan <= 120) {
                s_state.birth = old.birth; s_state.planning_age = old.plan;
            }
            nvs_close(h);
        }
    }
    s_jobs = xQueueCreate(6, sizeof(job_t));
    s_results = xQueueCreate(1, sizeof(result_t));
    if (!s_jobs || !s_results) return false;
    return xTaskCreate(worker, "quest_io", 4096, NULL, 3, NULL) == pdPASS;
}
void life_quest_app_enter(void)
{
    s_page = HOME; s_choice = 0; render();
    lv_timer_create(tick, 100, NULL);
}
bool life_quest_app_can_sleep(void) { return !s_pending; }
static unsigned clipped_content(lv_obj_t *parent)
{
    unsigned clipped = 0;
    for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;
        if (lv_obj_check_type(child, &lv_label_class) || lv_obj_check_type(child, &lv_image_class)) {
            lv_area_t outer, inner;
            lv_obj_get_content_coords(parent, &outer); lv_obj_get_coords(child, &inner);
            if (inner.x1 < outer.x1 || inner.y1 < outer.y1 || inner.x2 > outer.x2 || inner.y2 > outer.y2)
                ++clipped;
        }
        clipped += clipped_content(child);
    }
    return clipped;
}
void life_quest_app_command(const char *cmd)
{
    char verb[24], prefix[48], extra; unsigned request;
    snprintf(prefix, sizeof(prefix), "%s", cmd);
    if (sscanf(cmd, "%23s %u%c", verb, &request, &extra) == 2 &&
        (!strcmp(verb, "LQ_STATE") || !strcmp(verb, "LQ_LAYOUT") || !strcmp(verb, "LQ_PROFILE"))) {
        snprintf(prefix, sizeof(prefix), "%s %u", verb, request);
        cmd = verb; /* optional tagged queries; legacy untagged commands remain */
    }
    if (!strcmp(cmd, "LQ_LAYOUT")) {
        lv_obj_update_layout(lv_screen_active());
        (void)fap_console_printf("%s {\"page\":%d,\"clipped_text_or_images\":%u}\n",
               prefix, s_page, clipped_content(lv_screen_active()));
    } else if (!strcmp(cmd, "LQ_STATE")) {
        lv_mem_monitor_t mem; lv_mem_monitor(&mem);
        (void)fap_console_printf("%s {\"version\":\"1.3.0-life-quest\",\"page\":%d,\"choice\":%d,\"life_page\":%d,\"day\":%ld,\"latest_day\":%ld,\"xp\":%lu,\"count\":%u,\"hp\":%u,\"tipo_used\":%u,\"partner\":%u,\"sound\":%u,\"motion\":%u,\"clock_checked\":%s,\"pending\":%s,\"storage\":%s,\"audio_ready\":%s,\"tone_count\":%u,\"free_heap\":%lu,\"min_heap\":%lu,\"lv_free\":%lu,\"input_count\":%lu,\"last_input\":{\"request\":%lu,\"key\":%d,\"event\":%d,\"page\":%d,\"choice\":%d},\"untagged_inputs\":%lu,\"last_untagged_input\":{\"key\":%d,\"event\":%d,\"page\":%d,\"choice\":%d},\"adc_sample_errors\":%u,\"button_mv\":%d}\n",
            prefix, s_page, s_choice, s_life_page, (long)day_now(), (long)s_state.latest_day,
            (unsigned long)life_quest_xp(&s_state), life_quest_count(&s_state, day_now()),
            life_quest_hp(&s_state, day_now()), s_state.tipo_used, s_state.partner, s_state.sound, s_state.motion,
            s_clock_checked ? "true" : "false", s_pending ? "true" : "false",
            s_storage ? "true" : "false", atomic_load(&s_audio) ? "true" : "false", atomic_load(&s_tone_count),
            (unsigned long)esp_get_free_heap_size(),
            (unsigned long)esp_get_minimum_free_heap_size(), (unsigned long)mem.free_size,
            (unsigned long)s_input_count, (unsigned long)s_input_request,
            s_input_key, s_input_event, s_input_page, s_input_choice,
            (unsigned long)s_physical_count, s_physical_key, s_physical_event,
            s_physical_page, s_physical_choice, bsp_button_sample_error_count(), bsp_button_read_mv());
    } else if (!strcmp(cmd, "LQ_PROFILE")) {
        life_countdown_metrics_t m; if (!metrics(&m)) return;
        life_date_t d = today();
        (void)fap_console_printf("%s {\"schema\":\"life-quest/profile-v1\",\"name\":\"%s\",\"birthday\":\"%04u-%02u-%02u\",\"age_years\":%u,\"life_tokens_used\":%lu,\"life_tokens_to_plan\":%lu,\"planning_age\":%u,\"as_of\":\"%04u-%02u-%02u\",\"unit\":\"1 LT = 1 minute (custom time unit, not model tokens)\"}\n",
            prefix, name(), s_state.birth.year, s_state.birth.month, s_state.birth.day,
            m.age_years, (unsigned long)(m.lived_days * 1440U),
            (unsigned long)(m.remaining_days * 1440U), s_state.planning_age,
            d.year, d.month, d.day);
    } else if (!strncmp(cmd, "LQ_CLOCK ", 9) && !s_pending) {
        int y, mo, d, h, mi; char tail;
        if (sscanf(cmd + 9, "%4d-%2d-%2d %2d:%2d%c", &y, &mo, &d, &h, &mi, &tail) == 5) {
            struct tm t = {.tm_year = y - 1900, .tm_mon = mo - 1, .tm_mday = d, .tm_hour = h, .tm_min = mi};
            if (set_clock(t)) { s_page = HOME; s_choice = 0; toast("日期已通过本地连接校准"); (void)fap_console_printf("LQ_CLOCK OK\n"); }
            else (void)fap_console_printf("LQ_CLOCK INVALID\n");
        } else (void)fap_console_printf("LQ_CLOCK INVALID\n");
    }
}
