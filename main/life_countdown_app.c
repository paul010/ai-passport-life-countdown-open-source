#include "life_countdown_app.h"

#include "bsp_display.h"
#include "life_countdown_model.h"
#include "life_profile.h"
#include "nvs.h"
#include "profile_images.h"
#include "ui_pixel.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <stdio.h>
#include <time.h>

#define PROFILE_MAGIC 0x4C494645U
#define PROFILE_NAMESPACE "lifeclock"
#define PROFILE_KEY "profile"
#define PLAN_AGE_MIN 50U
#define PLAN_AGE_MAX 120U
#define BIRTH_YEAR_MIN 1900U
#define BIRTH_YEAR_MAX 2099U

static const char *TAG = "life_countdown";

typedef struct {
    uint32_t magic;
    life_date_t birth_date;
    uint8_t planning_age;
    uint8_t reserved[3];
} saved_profile_t;

typedef struct {
    uint32_t request_id;
    saved_profile_t profile;
} save_request_t;

typedef struct {
    uint32_t request_id;
    bool success;
} save_result_t;

typedef enum {
    LIFE_PAGE_OVERVIEW = 0,
    LIFE_PAGE_NUMBERS,
    LIFE_PAGE_EDIT,
} life_page_t;

typedef enum {
    LIFE_FIELD_YEAR = 0,
    LIFE_FIELD_MONTH,
    LIFE_FIELD_DAY,
    LIFE_FIELD_PLAN_AGE,
    LIFE_FIELD_COUNT,
} life_edit_field_t;

typedef enum {
    LIFE_SAVE_NONE = 0,
    LIFE_SAVE_PENDING,
    LIFE_SAVE_COMPLETE,
    LIFE_SAVE_FAILED,
} life_save_status_t;

static int s_battery_percent = -1;
static saved_profile_t s_profile = {
    .magic = PROFILE_MAGIC,
    .birth_date = {LIFE_PROFILE_BIRTH_YEAR, LIFE_PROFILE_BIRTH_MONTH,
                   LIFE_PROFILE_BIRTH_DAY},
    .planning_age = LIFE_PROFILE_PLAN_AGE,
};
static saved_profile_t s_draft;
static saved_profile_t s_pending_profile;
static life_page_t s_page = LIFE_PAGE_OVERVIEW;
static life_edit_field_t s_edit_field = LIFE_FIELD_YEAR;
static life_save_status_t s_save_status = LIFE_SAVE_NONE;
static uint32_t s_next_request_id;
static uint32_t s_pending_request_id;
static QueueHandle_t s_save_requests;
static QueueHandle_t s_save_results;
static bool s_storage_ready;
static lv_timer_t *s_refresh_timer;

static bool profile_is_valid(const saved_profile_t *profile)
{
    return profile != NULL && profile->magic == PROFILE_MAGIC &&
           life_countdown_date_is_valid(profile->birth_date) &&
           profile->planning_age >= PLAN_AGE_MIN &&
           profile->planning_age <= PLAN_AGE_MAX;
}

static void load_profile(void)
{
    nvs_handle_t handle;
    if (nvs_open(PROFILE_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }

    saved_profile_t loaded;
    size_t size = sizeof(loaded);
    if (nvs_get_blob(handle, PROFILE_KEY, &loaded, &size) == ESP_OK &&
        size == sizeof(loaded) && profile_is_valid(&loaded)) {
        s_profile = loaded;
    }
    nvs_close(handle);
}

static bool save_profile_to_nvs(const saved_profile_t *profile)
{
    nvs_handle_t handle;
    esp_err_t result = nvs_open(PROFILE_NAMESPACE, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return false;
    }
    result = nvs_set_blob(handle, PROFILE_KEY, profile, sizeof(*profile));
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result == ESP_OK;
}

/* NVS commits run outside both the button callback and the LVGL task. */
static void save_worker(void *argument)
{
    (void)argument;
    save_request_t request;
    for (;;) {
        if (xQueueReceive(s_save_requests, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        save_result_t result = {
            .request_id = request.request_id,
            .success = save_profile_to_nvs(&request.profile),
        };
        (void)xQueueOverwrite(s_save_results, &result);
    }
}

static void initialize_storage_worker(void)
{
    s_save_requests = xQueueCreate(1, sizeof(save_request_t));
    s_save_results = xQueueCreate(1, sizeof(save_result_t));
    if (s_save_requests == NULL || s_save_results == NULL ||
        xTaskCreate(save_worker, "life_save", 3072, NULL, 3, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Profile persistence is unavailable; edits remain local");
        if (s_save_requests != NULL) {
            vQueueDelete(s_save_requests);
            s_save_requests = NULL;
        }
        if (s_save_results != NULL) {
            vQueueDelete(s_save_results);
            s_save_results = NULL;
        }
        return;
    }
    s_storage_ready = true;
}

static bool today_from_clock(life_date_t *today)
{
    time_t now = time(NULL);
    struct tm local;
    if (today == NULL || now <= 0 || localtime_r(&now, &local) == NULL) {
        return false;
    }
    today->year = (uint16_t)(local.tm_year + 1900);
    today->month = (uint8_t)(local.tm_mon + 1);
    today->day = (uint8_t)local.tm_mday;
    return life_countdown_date_is_valid(*today);
}

static void format_number(uint32_t value, char *output, size_t output_size)
{
    char digits[16];
    int length = snprintf(digits, sizeof(digits), "%lu", (unsigned long)value);
    if (length <= 0 || (size_t)length >= sizeof(digits) || output_size == 0U) {
        if (output_size > 0U) {
            output[0] = '\0';
        }
        return;
    }

    size_t source = 0U;
    size_t target = 0U;
    while (source < (size_t)length && target + 1U < output_size) {
        if (source > 0U && ((size_t)length - source) % 3U == 0U) {
            output[target++] = ',';
            if (target + 1U >= output_size) {
                break;
            }
        }
        output[target++] = digits[source++];
    }
    output[target] = '\0';
}

static void add_battery(lv_obj_t *screen)
{
    if (s_battery_percent < 0) {
        return;
    }
    lv_obj_t *label = ui_pixel_label(screen, "", &lv_font_montserrat_12,
                                     UI_PAPER);
    lv_label_set_text_fmt(label, "%d%%", s_battery_percent);
    lv_obj_align(label, LV_ALIGN_TOP_RIGHT, -8, 47);
}

static lv_obj_t *begin_screen(const char *title)
{
    lv_obj_t *old_screen = lv_screen_active();
    if (old_screen != NULL) {
        lv_obj_clean(old_screen);
    }
    lv_obj_t *screen = ui_pixel_screen_create(title);
    lv_screen_load(screen);
    if (old_screen != NULL && old_screen != screen) {
        lv_obj_delete(old_screen);
    }
    add_battery(screen);
    return screen;
}

static lv_obj_t *positioned_label(lv_obj_t *screen, const char *text,
                                  const lv_font_t *font, uint32_t color,
                                  int x, int y)
{
    lv_obj_t *label = ui_pixel_label(screen, text, font, color);
    lv_obj_set_pos(label, x, y);
    return label;
}

static void add_footer(lv_obj_t *screen, const char *first_line,
                       const char *second_line)
{
    positioned_label(screen, first_line, &lv_font_montserrat_12, UI_INK, 7, 288);
    positioned_label(screen, second_line, &lv_font_montserrat_12, UI_INK, 7, 304);
}

static void add_avatar(lv_obj_t *screen)
{
    lv_obj_t *frame = ui_pixel_panel_create(screen, 12, 52, 54, 72, UI_PAPER);
    lv_obj_set_style_pad_all(frame, 0, 0);
    lv_obj_set_style_border_width(frame, 3, 0);

    lv_obj_t *image = lv_image_create(screen);
    lv_image_set_src(image, &profile_avatar_image);
    lv_image_set_scale(image, 171);
    lv_obj_set_pos(image, 15, 56);
}

static bool calculate_metrics(life_date_t *today,
                              life_countdown_metrics_t *metrics)
{
    return today_from_clock(today) &&
           life_countdown_calculate(s_profile.birth_date,
                                    s_profile.planning_age, *today, metrics);
}

static void render_overview(void)
{
    life_date_t today;
    life_countdown_metrics_t metrics;
    lv_obj_t *screen = begin_screen("LIFE TIMER");
    if (!calculate_metrics(&today, &metrics)) {
        positioned_label(screen, "DEVICE DATE UNAVAILABLE",
                         &lv_font_montserrat_20, UI_PAPER, 12, 92);
        add_footer(screen, "CHECK DEVICE CLOCK", "HOLD OK HOME");
        return;
    }

    add_avatar(screen);
    positioned_label(screen, LIFE_PROFILE_NAME, &lv_font_montserrat_20,
                     UI_PAPER, 75, 53);
    positioned_label(screen, LIFE_PROFILE_ROLE, &lv_font_montserrat_12, UI_PAPER,
                     75, 79);
    positioned_label(screen, "BORN", &lv_font_montserrat_12, UI_PAPER, 75, 96);
    lv_obj_t *birth = positioned_label(screen, "", &lv_font_montserrat_12,
                                       UI_PAPER, 75, 111);
    lv_label_set_text_fmt(birth, "%04u/%02u/%02u", s_profile.birth_date.year,
                          s_profile.birth_date.month, s_profile.birth_date.day);

    ui_pixel_panel_create(screen, 12, 132, 214, 68, UI_PAPER);
    positioned_label(screen, "DAYS TO YOUR PERSONAL PLAN",
                     &lv_font_montserrat_12, UI_INK, 18, 139);
    char remaining[16];
    format_number(metrics.remaining_days, remaining, sizeof(remaining));
    positioned_label(screen, remaining, &lv_font_montserrat_20, UI_INK, 17, 151);
    lv_obj_t *plan = positioned_label(screen, "", &lv_font_montserrat_12,
                                      UI_SKY_DARK, 122, 172);
    lv_label_set_text_fmt(plan, "TO AGE %u", s_profile.planning_age);

    lv_obj_t *bar = lv_bar_create(screen);
    lv_obj_set_pos(bar, 16, 209);
    lv_obj_set_size(bar, 208, 11);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, metrics.progress_percent, LV_ANIM_OFF);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(bar, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, lv_color_hex(UI_INK), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(UI_PAPER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(UI_GRASS), LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);

    lv_obj_t *age = positioned_label(screen, "", &lv_font_montserrat_12,
                                     UI_INK, 16, 222);
    lv_label_set_text_fmt(age, "AGE %u / %u YEARS", metrics.age_years,
                          s_profile.planning_age);
    char lived[16];
    char weeks[16];
    char months[16];
    format_number(metrics.lived_days, lived, sizeof(lived));
    format_number(metrics.lived_weeks, weeks, sizeof(weeks));
    format_number(metrics.age_months, months, sizeof(months));
    lv_obj_t *stats_one = positioned_label(screen, "", &lv_font_montserrat_12,
                                           UI_PAPER, 14, 242);
    lv_label_set_text_fmt(stats_one, "LIVED %sd", lived);
    lv_obj_t *stats_two = positioned_label(screen, "", &lv_font_montserrat_12,
                                           UI_PAPER, 126, 242);
    lv_label_set_text_fmt(stats_two, "WEEKS %s", weeks);
    lv_obj_t *stats_three = positioned_label(screen, "", &lv_font_montserrat_12,
                                             UI_PAPER, 14, 260);
    lv_label_set_text_fmt(stats_three, "MONTHS %s", months);
    positioned_label(screen, "UP/DN NUMBERS  OK EDIT",
                     &lv_font_montserrat_12, UI_INK, 7, 288);
    const char *status = s_save_status == LIFE_SAVE_PENDING
                             ? "SAVING PROFILE..."
                             : s_save_status == LIFE_SAVE_COMPLETE
                                   ? "PROFILE SAVED"
                                   : s_save_status == LIFE_SAVE_FAILED
                                         ? "SAVE FAILED - EDIT TO RETRY"
                                         : "HOLD OK HOME";
    positioned_label(screen, status, &lv_font_montserrat_12, UI_INK, 7, 304);
}

static void add_stat_card(lv_obj_t *screen, int x, int y, const char *title,
                          const char *value)
{
    ui_pixel_panel_create(screen, x, y, 103, 54, UI_PAPER);
    lv_obj_t *heading = positioned_label(screen, title, &lv_font_montserrat_12,
                                         UI_SKY_DARK, x + 5, y + 7);
    lv_obj_set_width(heading, 93);
    lv_obj_set_style_text_align(heading, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *number = positioned_label(screen, value, &lv_font_montserrat_14,
                                        UI_INK, x + 5, y + 28);
    lv_obj_set_width(number, 93);
    lv_obj_set_style_text_align(number, LV_TEXT_ALIGN_CENTER, 0);
}

static void render_numbers(void)
{
    life_date_t today;
    life_countdown_metrics_t metrics;
    lv_obj_t *screen = begin_screen("LIFE NUMBERS");
    if (!calculate_metrics(&today, &metrics)) {
        positioned_label(screen, "DEVICE DATE UNAVAILABLE",
                         &lv_font_montserrat_20, UI_PAPER, 12, 92);
        add_footer(screen, "CHECK DEVICE CLOCK", "HOLD OK HOME");
        return;
    }

    char months[16];
    char weeks[16];
    char days[16];
    format_number(metrics.age_months, months, sizeof(months));
    format_number(metrics.lived_weeks, weeks, sizeof(weeks));
    format_number(metrics.lived_days, days, sizeof(days));
    char value[24];
    (void)snprintf(value, sizeof(value), "%u YEARS", metrics.age_years);
    add_stat_card(screen, 12, 56, "AGE", value);
    (void)snprintf(value, sizeof(value), "%s MONTHS", months);
    add_stat_card(screen, 125, 56, "TIME LIVED", value);
    (void)snprintf(value, sizeof(value), "%s WEEKS", weeks);
    add_stat_card(screen, 12, 119, "WEEKS LIVED", value);
    (void)snprintf(value, sizeof(value), "%s DAYS", days);
    add_stat_card(screen, 125, 119, "DAYS LIVED", value);

    positioned_label(screen, "YOUR PLANNING LINE IS NOT A",
                     &lv_font_montserrat_12, UI_PAPER, 14, 190);
    positioned_label(screen, "LIFESPAN OR MEDICAL PREDICTION.",
                     &lv_font_montserrat_12, UI_PAPER, 14, 206);
    lv_obj_t *birth = positioned_label(screen, "", &lv_font_montserrat_12,
                                       UI_PAPER, 14, 231);
    lv_label_set_text_fmt(birth, "BORN %04u/%02u/%02u",
                          s_profile.birth_date.year,
                          s_profile.birth_date.month,
                          s_profile.birth_date.day);
    lv_obj_t *date = positioned_label(screen, "", &lv_font_montserrat_12,
                                      UI_PAPER, 14, 248);
    lv_label_set_text_fmt(date, "TODAY %04u/%02u/%02u",
                          today.year, today.month, today.day);
    add_footer(screen, "UP/DN OVERVIEW  OK EDIT", "HOLD OK HOME");
}

static void render_edit(void)
{
    lv_obj_t *screen = begin_screen("EDIT DATE");
    static const char *const labels[LIFE_FIELD_COUNT] = {
        "YEAR", "MONTH", "DAY", "PLAN",
    };
    const int x_positions[LIFE_FIELD_COUNT] = {12, 66, 120, 174};
    for (int i = 0; i < LIFE_FIELD_COUNT; ++i) {
        int x = x_positions[i];
        bool selected = (life_edit_field_t)i == s_edit_field;
        lv_obj_t *cell = ui_pixel_panel_create(screen, x, 60, 50, 70,
                                                selected ? UI_YELLOW : UI_PAPER);
        lv_obj_set_style_border_width(cell, 2, 0);
        lv_obj_t *label = positioned_label(screen, labels[i],
                                           &lv_font_montserrat_12, UI_SKY_DARK,
                                           x + 3, 69);
        lv_obj_set_width(label, 44);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        char value[8];
        if (i == LIFE_FIELD_YEAR) {
            (void)snprintf(value, sizeof(value), "%04u", s_draft.birth_date.year);
        } else if (i == LIFE_FIELD_MONTH) {
            (void)snprintf(value, sizeof(value), "%02u", s_draft.birth_date.month);
        } else if (i == LIFE_FIELD_DAY) {
            (void)snprintf(value, sizeof(value), "%02u", s_draft.birth_date.day);
        } else {
            (void)snprintf(value, sizeof(value), "%uY", s_draft.planning_age);
        }
        lv_obj_t *field_value = positioned_label(screen, value,
                                                 &lv_font_montserrat_14,
                                                 UI_INK, x + 3, 96);
        lv_obj_set_width(field_value, 44);
        lv_obj_set_style_text_align(field_value, LV_TEXT_ALIGN_CENTER, 0);
    }

    positioned_label(screen, "BIRTH DATE", &lv_font_montserrat_12,
                     UI_PAPER, 14, 148);
    lv_obj_t *birth = positioned_label(screen, "", &lv_font_montserrat_20,
                                       UI_PAPER, 14, 164);
    lv_label_set_text_fmt(birth, "%04u-%02u-%02u", s_draft.birth_date.year,
                          s_draft.birth_date.month, s_draft.birth_date.day);
    positioned_label(screen, "UP/DN CHANGE VALUE", &lv_font_montserrat_14,
                     UI_PAPER, 14, 198);
    positioned_label(screen,
                     s_edit_field == LIFE_FIELD_PLAN_AGE ? "OK SAVE PROFILE"
                                                        : "OK NEXT FIELD",
                     &lv_font_montserrat_14, UI_PAPER, 14, 218);
    positioned_label(screen, "PLANNING LINE, NOT A PREDICTION",
                     &lv_font_montserrat_12, UI_PAPER, 14, 244);
    const char *status = s_save_status == LIFE_SAVE_PENDING
                             ? "SAVING ON DEVICE..."
                             : s_save_status == LIFE_SAVE_FAILED
                                   ? "SAVE FAILED - PRESS OK TO RETRY"
                                   : "HOLD OK HOME TO CANCEL";
    positioned_label(screen, status, &lv_font_montserrat_12, UI_YELLOW,
                     14, 262);
    add_footer(screen, "UP/DN CHANGE  OK NEXT", "HOLD OK HOME");
}

static void render_current_page(void)
{
    switch (s_page) {
    case LIFE_PAGE_NUMBERS:
        render_numbers();
        break;
    case LIFE_PAGE_EDIT:
        render_edit();
        break;
    case LIFE_PAGE_OVERVIEW:
    default:
        render_overview();
        break;
    }
}

static void poll_save_result(void)
{
    if (s_save_results == NULL) {
        return;
    }
    save_result_t result;
    if (xQueueReceive(s_save_results, &result, 0) != pdTRUE ||
        result.request_id != s_pending_request_id) {
        return;
    }

    if (result.success) {
        s_profile = s_pending_profile;
        s_page = LIFE_PAGE_OVERVIEW;
        s_save_status = LIFE_SAVE_COMPLETE;
    } else {
        s_page = LIFE_PAGE_EDIT;
        s_edit_field = LIFE_FIELD_PLAN_AGE;
        s_save_status = LIFE_SAVE_FAILED;
    }
    render_current_page();
}

static void refresh_state(lv_timer_t *timer)
{
    (void)timer;
    poll_save_result();
}

static void adjust_draft(int direction)
{
    if (s_edit_field == LIFE_FIELD_YEAR) {
        int year = (int)s_draft.birth_date.year + direction;
        if (year >= (int)BIRTH_YEAR_MIN && year <= (int)BIRTH_YEAR_MAX) {
            s_draft.birth_date.year = (uint16_t)year;
        }
    } else if (s_edit_field == LIFE_FIELD_MONTH) {
        int month = (int)s_draft.birth_date.month + direction;
        if (month < 1) {
            month = 12;
        } else if (month > 12) {
            month = 1;
        }
        s_draft.birth_date.month = (uint8_t)month;
    } else if (s_edit_field == LIFE_FIELD_DAY) {
        uint8_t last_day = life_countdown_days_in_month(
            s_draft.birth_date.year, s_draft.birth_date.month);
        int day = (int)s_draft.birth_date.day + direction;
        if (day < 1) {
            day = last_day;
        } else if (day > last_day) {
            day = 1;
        }
        s_draft.birth_date.day = (uint8_t)day;
    } else if (s_edit_field == LIFE_FIELD_PLAN_AGE) {
        int age = (int)s_draft.planning_age + direction;
        if (age >= (int)PLAN_AGE_MIN && age <= (int)PLAN_AGE_MAX) {
            s_draft.planning_age = (uint8_t)age;
        }
    }

    uint8_t last_day = life_countdown_days_in_month(
        s_draft.birth_date.year, s_draft.birth_date.month);
    if (s_draft.birth_date.day > last_day) {
        s_draft.birth_date.day = last_day;
    }
}

static void submit_profile(void)
{
    if (!s_storage_ready || s_save_requests == NULL) {
        s_save_status = LIFE_SAVE_FAILED;
        render_edit();
        return;
    }

    s_pending_profile = s_draft;
    save_request_t request = {
        .request_id = ++s_next_request_id,
        .profile = s_pending_profile,
    };
    if (xQueueSend(s_save_requests, &request, 0) != pdTRUE) {
        s_save_status = LIFE_SAVE_FAILED;
        render_edit();
        return;
    }

    s_pending_request_id = request.request_id;
    s_save_status = LIFE_SAVE_PENDING;
    render_edit();
}

bool life_countdown_app_init(int battery_percent)
{
    s_battery_percent = battery_percent;
    load_profile();
    if (!s_storage_ready && s_save_requests == NULL && s_save_results == NULL) {
        initialize_storage_worker();
    }
    return true;
}

void life_countdown_app_enter(void)
{
    s_page = LIFE_PAGE_OVERVIEW;
    if (s_save_status != LIFE_SAVE_PENDING) {
        s_save_status = LIFE_SAVE_NONE;
    }
    render_current_page();
    if (s_refresh_timer == NULL) {
        s_refresh_timer = lv_timer_create(refresh_state, 1000, NULL);
    }
}

void life_countdown_app_exit(void)
{
    if (s_refresh_timer != NULL) {
        lv_timer_delete(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    s_page = LIFE_PAGE_OVERVIEW;
    s_edit_field = LIFE_FIELD_YEAR;
}

bool life_countdown_app_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (button == BSP_BTN_OK && event == BSP_BTN_LONG) {
        return true;
    }
    if (event != BSP_BTN_CLICK) {
        return false;
    }
    if (s_save_status == LIFE_SAVE_PENDING) {
        return false;
    }

    if (s_page == LIFE_PAGE_EDIT) {
        if (button == BSP_BTN_UP || button == BSP_BTN_DOWN) {
            adjust_draft(button == BSP_BTN_UP ? 1 : -1);
            s_save_status = LIFE_SAVE_NONE;
            render_edit();
        } else if (button == BSP_BTN_OK) {
            if (s_edit_field == LIFE_FIELD_PLAN_AGE) {
                submit_profile();
            } else {
                s_edit_field = (life_edit_field_t)(s_edit_field + 1);
                s_save_status = LIFE_SAVE_NONE;
                render_edit();
            }
        }
        return false;
    }

    if (button == BSP_BTN_UP || button == BSP_BTN_DOWN) {
        s_page = s_page == LIFE_PAGE_OVERVIEW ? LIFE_PAGE_NUMBERS
                                              : LIFE_PAGE_OVERVIEW;
        s_save_status = LIFE_SAVE_NONE;
        render_current_page();
    } else if (button == BSP_BTN_OK) {
        s_draft = s_profile;
        s_edit_field = LIFE_FIELD_YEAR;
        s_save_status = LIFE_SAVE_NONE;
        s_page = LIFE_PAGE_EDIT;
        render_edit();
    }
    return false;
}
