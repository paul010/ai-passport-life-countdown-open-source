// Standalone Life Quest firmware entry point; no AB-731 app is compiled.
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "idle_power.h"
#include "life_quest_app.h"
#include "fap_console.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "life_main";
#define IDLE_SLEEP_MS (5U * 60U * 1000U)

static volatile uint32_t s_last_activity_ms;
static bool s_accept_buttons;
static QueueHandle_t s_button_queue;

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
    bool diagnostic;
    uint32_t request;
} button_message_t;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void note_activity(void)
{
    s_last_activity_ms = now_ms();
}

static void enter_idle_sleep(void)
{
    if (bsp_button_any_pressed() || !life_quest_app_can_sleep() || fap_console_binary_busy()) {
        note_activity();
        return;
    }

    esp_err_t result = esp_deep_sleep_enable_gpio_wakeup(
        1ULL << BSP_BTN_GPIO, ESP_GPIO_WAKEUP_GPIO_LOW);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Could not arm button wake source: %s",
                 esp_err_to_name(result));
        note_activity();
        return;
    }

    if (bsp_lvgl_lock(1000)) {
        result = bsp_display_sleep();
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "LCD sleep returned: %s", esp_err_to_name(result));
        }
        bsp_display_backlight(0);
        bsp_lvgl_unlock();
    } else {
        bsp_display_backlight(0);
    }

    result = bsp_battery_sleep();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Battery-gauge sleep returned: %s",
                 esp_err_to_name(result));
    }
    result = bsp_button_deinit();
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Button shutdown returned: %s", esp_err_to_name(result));
    }
    ESP_LOGI(TAG, "Five minutes idle; entering deep sleep (any key wakes)");
    esp_deep_sleep_start();
}

/* The Passport has no external real-time clock and this offline app does not
 * connect to Wi-Fi. Seed the software clock from the firmware build time so
 * the home screen can still show a useful local date and time after boot. */
static void initialize_offline_clock(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();
    time_t now = time(NULL);
    if (now >= 1704067200) {
        return;
    }

    static const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char month_name[4] = { __DATE__[0], __DATE__[1], __DATE__[2], '\0' };
    const char *month = strstr(months, month_name);
    struct tm build_time = {
        .tm_year = atoi(__DATE__ + 7) - 1900,
        .tm_mon = month ? (int)((month - months) / 3) : 0,
        .tm_mday = atoi(__DATE__ + 4),
        .tm_hour = atoi(__TIME__),
        .tm_min = atoi(__TIME__ + 3),
        .tm_sec = atoi(__TIME__ + 6),
        .tm_isdst = -1,
    };

    time_t compiled_at = mktime(&build_time);
    if (compiled_at > 0) {
        struct timeval value = { .tv_sec = compiled_at, .tv_usec = 0 };
        settimeofday(&value, NULL);
    }
}

// Button callbacks run in the esp_timer task and must never redraw LVGL. Queue
// an immutable event for app_main, which owns all navigation and screen work.
static void button_callback(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    button_message_t message = { .button = button, .event = event };
    if (s_button_queue != NULL) {
        (void)xQueueSend(s_button_queue, &message, 0);
    }
}

static bool handle_key(const button_message_t *message)
{
    bsp_btn_t button = message->button;
    bsp_btn_ev_t event = message->event;
    note_activity();
    if (!s_accept_buttons) {
        return !message->diagnostic; /* diagnostics wait for wake-key release */
    }
    if (event == BSP_BTN_PRESS) {
        ESP_LOGI(TAG, "Button press classified as key=%d, ADC=%d mV",
                 (int)button, bsp_button_read_mv());
        return true;
    }
    if (event != BSP_BTN_CLICK && event != BSP_BTN_LONG) return true;
    if (!bsp_lvgl_lock(250)) {
        /* A busy renderer/capture must not silently eat a released key. The
         * application loop retains this event and retries in order. */
        return false;
    }
    if (!life_quest_app_can_sleep()) {
        bsp_lvgl_unlock();
        return false; /* retain the key until the outstanding save completes */
    }
    life_quest_app_key(button, event, message->diagnostic ? message->request : 0);
    bsp_lvgl_unlock();
    return true;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting standalone Life Quest 1.3.0-life-quest");
    bool woke_from_button =
        esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
    initialize_offline_clock();

    esp_err_t nvs_result = nvs_flash_init();
    /* Never silently erase another app's settings or provisioned identity.
     * Show a save failure if NVS is incompatible; diagnostics remain usable. */
    if (nvs_result != ESP_OK) {
        ESP_LOGE(TAG, "NVS unavailable, preserved without erase: %s", esp_err_to_name(nvs_result));
    }

    // I2C only serves the optional battery gauge here. A gauge failure must not
    // prevent the display-and-buttons countdown from starting.
    (void)bsp_i2c_init();
    if (bsp_display_init() != ESP_OK || bsp_lvgl_init() == NULL) {
        ESP_LOGE(TAG,
                 "Display/LVGL init failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    int battery_percent = -1;
    if (bsp_battery_init() == ESP_OK) {
        battery_percent = bsp_battery_soc();
    }

    if (!life_quest_app_init(battery_percent, nvs_result == ESP_OK,
                            esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED)) {
        ESP_LOGE(TAG, "Life Quest initialization failed");
        return;
    }
    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "Could not lock LVGL for countdown home");
        return;
    }
    life_quest_app_enter();
    bsp_lvgl_unlock();
    s_button_queue = xQueueCreate(8, sizeof(button_message_t));
    if (s_button_queue == NULL) {
        ESP_LOGE(TAG, "Could not create button event queue");
        return;
    }
    ESP_ERROR_CHECK(bsp_button_init(button_callback, NULL));
    s_accept_buttons = !woke_from_button;
    note_activity();
    if (!fap_console_init()) ESP_LOGW(TAG, "Local USB diagnostics unavailable");
    ESP_LOGI(TAG, "Life Quest home ready; cold boot requires date confirmation");

    button_message_t deferred_key = {0};
    bool has_deferred_key = false;
    for (;;) {
        bool pressed = bsp_button_any_pressed();
        if (!s_accept_buttons && !pressed) {
            s_accept_buttons = true;
            note_activity();
        }

        if (!has_deferred_key) {
            has_deferred_key = xQueueReceive(s_button_queue, &deferred_key,
                                             pdMS_TO_TICKS(100)) == pdTRUE;
        }
        if (has_deferred_key && handle_key(&deferred_key)) {
            if (deferred_key.diagnostic) {
                if (deferred_key.request) {
                    const char *key_name = deferred_key.event == BSP_BTN_LONG ? "HOME" :
                        deferred_key.button == BSP_BTN_UP ? "UP" :
                        deferred_key.button == BSP_BTN_DOWN ? "DOWN" : "OK";
                    (void)fap_console_printf("LQ_KEY_ACK %lu %s\n", (unsigned long)deferred_key.request, key_name);
                } else (void)fap_console_printf("LQ_KEY OK\n");
            }
            has_deferred_key = false;
        }
        char command[80];
        if (fap_console_next(command, sizeof(command))) {
            /* Diagnostic keys exercise the same queued application path, but
             * are not evidence of the physical resistor-ladder input. */
            if (!strncmp(command, "LQ_KEY ", 7)) {
                button_message_t test = {.event = BSP_BTN_CLICK, .diagnostic = true};
                char key_name[8], extra; unsigned request = 0;
                int parts = sscanf(command + 7, "%7s %u%c", key_name, &request, &extra);
                test.request = parts == 2 ? request : 0;
                bool known = true;
                if (parts < 1 || parts > 2) known = false;
                else if (!strcmp(key_name, "UP")) test.button = BSP_BTN_UP;
                else if (!strcmp(key_name, "DOWN")) test.button = BSP_BTN_DOWN;
                else if (!strcmp(key_name, "OK")) test.button = BSP_BTN_OK;
                else if (!strcmp(key_name, "HOME")) { test.button = BSP_BTN_OK; test.event = BSP_BTN_LONG; }
                else known = false;
                if (known && xQueueSend(s_button_queue, &test, 0) != pdTRUE) {
                    if (test.request) (void)fap_console_printf("LQ_KEY_BUSY %lu\n", (unsigned long)test.request);
                    else (void)fap_console_printf("LQ_KEY BUSY\n");
                }
            } else if (!strcmp(command, "LQ_SLEEP")) {
                enter_idle_sleep();
            } else if (!strcmp(command, "LQ_REBOOT") && life_quest_app_can_sleep()) {
                esp_restart();
            } else if (bsp_lvgl_lock(1000)) {
                life_quest_app_command(command);
                bsp_lvgl_unlock();
                if (!strncmp(command, "LQ_CLOCK ", 9)) note_activity();
            }
        }
        if (s_accept_buttons && !has_deferred_key &&
            idle_power_expired(s_last_activity_ms, now_ms(), IDLE_SLEEP_MS)) {
            enter_idle_sleep();
        }
    }
}
