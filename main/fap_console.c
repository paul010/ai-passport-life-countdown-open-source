#include "fap_console.h"
#include "bsp_display.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Reserve a tight RGB565 frame before runtime fragmentation. No card identity
 * is drawn by this app. Capture only reads the active screen. */
static uint8_t s_pixels[240 * 320 * 2] __attribute__((aligned(64)));
static QueueHandle_t s_commands;
static volatile bool s_binary;
static SemaphoreHandle_t s_tx_lock;
typedef struct { char text[80]; } command_t;

static bool write_bytes(const void *data, size_t length)
{
    const uint8_t *p = data;
    while (length) {
        size_t n = length > 512 ? 512 : length;
        int written = usb_serial_jtag_write_bytes(p, n, pdMS_TO_TICKS(2000));
        if (written <= 0) return false;
        p += written; length -= written;
    }
    return true;
}
bool fap_console_printf(const char *format, ...)
{
    char frame[1024];
    va_list args; va_start(args, format);
    int n = vsnprintf(frame, sizeof(frame), format, args);
    va_end(args);
    if (n < 0 || n >= (int)sizeof(frame) || !s_tx_lock ||
        xSemaphoreTake(s_tx_lock, pdMS_TO_TICKS(2000)) != pdTRUE) return false;
    /* IDF's character-at-a-time stdout path drops bytes after a short TX
     * timeout. Protocol frames must instead check every actual write. */
    flockfile(stdout);
    bool sent = write_bytes(frame, n);
    funlockfile(stdout);
    xSemaphoreGive(s_tx_lock);
    return sent;
}
static void capture(void)
{
    if (!bsp_lvgl_lock(2000)) return;
    lv_draw_buf_t buf;
    lv_result_t r = lv_draw_buf_init(&buf, 240, 320, LV_COLOR_FORMAT_RGB565,
                                   480, s_pixels, sizeof(s_pixels));
    if (r == LV_RESULT_OK)
        r = lv_snapshot_take_to_draw_buf(lv_screen_active(), LV_COLOR_FORMAT_RGB565, &buf);
    bool valid = r == LV_RESULT_OK && buf.header.w == 240 && buf.header.h == 320 &&
                 buf.header.stride == 480;
    bsp_lvgl_unlock();
    if (!valid) { ESP_LOGW("fap_console", "Snapshot unavailable"); return; }
    s_binary = true;
    esp_log_level_set("*", ESP_LOG_NONE);
    if (xSemaphoreTake(s_tx_lock, pdMS_TO_TICKS(2000)) != pdTRUE) {
        esp_log_level_set("*", ESP_LOG_INFO); s_binary = false; return;
    }
    flockfile(stdout);
    (void)usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(2000));
    const char header[] = "FAP_SCREENSHOT_V1 240 320 RGB565LE 153600\n";
    bool sent = write_bytes(header, sizeof(header) - 1);
    if (sent) sent = write_bytes(s_pixels, sizeof(s_pixels));
    (void)usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(2000));
    funlockfile(stdout);
    xSemaphoreGive(s_tx_lock);
    esp_log_level_set("*", ESP_LOG_INFO);
    s_binary = false;
    if (!sent) ESP_LOGW("fap_console", "Capture transfer stopped; reconnect to retry");
}
static void reader(void *arg)
{
    (void)arg;
    command_t cmd = {0}; unsigned used = 0;
    for (;;) {
        if (!usb_serial_jtag_is_driver_installed()) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        uint8_t ch;
        int n = usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(100));
        if (n <= 0) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
        if (ch == '\n' || ch == '\r') {
            if (used && !strncmp(cmd.text, "LQ_", 3)) (void)xQueueSend(s_commands, &cmd, 0);
            used = 0; memset(&cmd, 0, sizeof(cmd)); continue;
        }
        if (ch < 32 || ch > 126) continue;
        if (used == sizeof(cmd.text) - 1) { used = 0; memset(&cmd, 0, sizeof(cmd)); }
        cmd.text[used++] = (char)ch;
        if (strstr(cmd.text, "FAP_SCREENSHOT_V1")) {
            used = 0; memset(&cmd, 0, sizeof(cmd)); capture();
        }
    }
}
bool fap_console_init(void)
{
    usb_serial_jtag_driver_config_t cfg = {.rx_buffer_size = 256, .tx_buffer_size = 1024};
    if (!usb_serial_jtag_is_driver_installed() && usb_serial_jtag_driver_install(&cfg) != ESP_OK)
        return false;
    usb_serial_jtag_vfs_use_driver();
    s_tx_lock = xSemaphoreCreateMutex();
    s_commands = xQueueCreate(6, sizeof(command_t));
    return s_tx_lock && s_commands && xTaskCreate(reader, "local_console", 8192, NULL, 2, NULL) == pdPASS;
}
bool fap_console_binary_busy(void) { return s_binary; }
bool fap_console_next(char *command, size_t size)
{
    if (s_binary || !s_commands || !size) return false;
    command_t cmd;
    if (xQueueReceive(s_commands, &cmd, 0) != pdTRUE) return false;
    snprintf(command, size, "%s", cmd.text); return true;
}
