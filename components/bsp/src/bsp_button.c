// components/bsp/src/bsp_button.c
// 移植自 trae_card/components/platform/platform_esp32/src/btn_iot_button.c
#include "bsp_button.h"
#include "bsp_pins.h"
#include "bsp_button_sample.h"
#include "iot_button.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdatomic.h>

static const char *TAG = "bsp_btn";

static const uint16_t BTN_MV[BSP_BTN_COUNT][2] = BSP_BTN_MV_TABLE;

static button_handle_t s_btn[BSP_BTN_COUNT];
static bsp_btn_cb_t    s_cb;
static void           *s_user;

// One ADC1 owner serves both the checked ladder driver and voltage diagnostics.
static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t         s_cali;
typedef struct { button_driver_t base; unsigned index; } ladder_driver_t;
static ladder_driver_t s_driver[BSP_BTN_COUNT];
static int s_sample_mv = -1;
static int64_t s_sample_at;
static atomic_uint s_sample_errors;

// Preserve the original board's ADC attenuation and resistor-ladder thresholds.
#define BSP_BTN_ATTEN  ADC_ATTEN_DB_12       // 量程约 0~3100mV,覆盖松开态

/* Reuse Espressif's debounce/click/long-press state machine, with a checked
 * board-owned ADC source. Its generic ADC driver does not check read errors;
 * under snapshot load a failed conversion can otherwise fabricate UP=0 mV.
 * Three ladder keys share one valid sample within each polling pass. */
static uint8_t ladder_level(button_driver_t *base)
{
    ladder_driver_t *driver = (ladder_driver_t *)base;
    int64_t now = esp_timer_get_time();
    if (!s_sample_at || now - s_sample_at > 1000) {
        int batch[5];
        for (int i = 0; i < 5; ++i) batch[i] = bsp_button_read_mv();
        int mv = bsp_button_sample_median5(batch);
        if (mv < 0) atomic_fetch_add(&s_sample_errors, 1);
        s_sample_mv = bsp_button_sample_accept(s_sample_mv, mv >= 0, mv);
        s_sample_at = now;
    }
    return bsp_button_sample_in_window(s_sample_mv, BTN_MV[driver->index][0],
                                       BTN_MV[driver->index][1])
        ? BUTTON_ACTIVE : BUTTON_INACTIVE;
}
static esp_err_t ladder_delete(button_driver_t *base)
{
    (void)base; /* Static driver slots; ADC is released after all keys below. */
    return ESP_OK;
}

// 每个按键把"哪个键"随回调带回来。button 组件的回调签名固定,故用 usr_data 传索引。
static void on_event(void *arg, void *usr_data, bsp_btn_ev_t ev) {
    (void)arg;
    if (!s_cb) return;
    s_cb((bsp_btn_t)(intptr_t)usr_data, ev, s_user);
}
static void cb_press (void *a, void *u) { on_event(a, u, BSP_BTN_PRESS);  }
static void cb_click (void *a, void *u) { on_event(a, u, BSP_BTN_CLICK);  }
static void cb_double(void *a, void *u) { on_event(a, u, BSP_BTN_DOUBLE); }
static void cb_long  (void *a, void *u) { on_event(a, u, BSP_BTN_LONG);   }

esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user) {
    if (s_adc) return ESP_ERR_INVALID_STATE;
    s_cb = cb; s_user = user;

    // The BSP owns and configures the single unit before creating button events.
    const adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = BSP_BTN_ADC_UNIT };
    esp_err_t ae = adc_oneshot_new_unit(&ucfg, &s_adc);
    if (ae != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit 创建失败 (%s)", esp_err_to_name(ae));
        s_adc = NULL;
        return ae;
    }

    const adc_oneshot_chan_cfg_t channel = {
        .bitwidth = ADC_BITWIDTH_DEFAULT, .atten = BSP_BTN_ATTEN,
    };
    ae = adc_oneshot_config_channel(s_adc, BSP_BTN_ADC_CHANNEL, &channel);
    if (ae != ESP_OK) { bsp_button_deinit(); return ae; }

    // One calibrated, checked ADC source serves polling and live diagnostics.
    const adc_cali_curve_fitting_config_t cal = {
        .unit_id  = BSP_BTN_ADC_UNIT,
        .chan     = BSP_BTN_ADC_CHANNEL,
        .atten    = BSP_BTN_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ae = adc_cali_create_scheme_curve_fitting(&cal, &s_cali);
    if (ae != ESP_OK) {
        ESP_LOGE(TAG, "ADC 校准创建失败，拒绝把无效采样当作按键");
        s_cali = NULL;
        bsp_button_deinit();
        return ae;
    }
    s_sample_mv = -1; s_sample_at = 0; atomic_store(&s_sample_errors, 0);
    for (int i = 0; i < BSP_BTN_COUNT; i++) {
        s_driver[i] = (ladder_driver_t){
            .base = {.get_key_level = ladder_level, .del = ladder_delete}, .index = i,
        };
        const button_config_t bc = {0};
        esp_err_t e = iot_button_create(&bc, &s_driver[i].base, &s_btn[i]);
        if (e != ESP_OK || !s_btn[i]) {
            bsp_button_deinit();
            return e == ESP_OK ? ESP_FAIL : e;
        }
        void *idx = (void *)(intptr_t)i;
        e = iot_button_register_cb(s_btn[i], BUTTON_PRESS_DOWN, NULL, cb_press, idx);
        if (e == ESP_OK) e = iot_button_register_cb(s_btn[i], BUTTON_SINGLE_CLICK, NULL, cb_click, idx);
        if (e == ESP_OK) e = iot_button_register_cb(s_btn[i], BUTTON_DOUBLE_CLICK, NULL, cb_double, idx);
        if (e == ESP_OK) e = iot_button_register_cb(s_btn[i], BUTTON_LONG_PRESS_START, NULL, cb_long, idx);
        if (e != ESP_OK) { bsp_button_deinit(); return e; }
    }

    ESP_LOGI(TAG, "按键就绪:ADC1_CH%d 三键分压", BSP_BTN_ADC_CHANNEL);
    return ESP_OK;
}

esp_err_t bsp_button_deinit(void) {
    esp_err_t first_error = ESP_OK;
    s_cb = NULL;
    s_user = NULL;

    for (int i = 0; i < BSP_BTN_COUNT; i++) {
        if (!s_btn[i]) continue;
        esp_err_t err = iot_button_delete(s_btn[i]);
        if (first_error == ESP_OK && err != ESP_OK) first_error = err;
        s_btn[i] = NULL;
    }
    if (s_cali) {
        esp_err_t err = adc_cali_delete_scheme_curve_fitting(s_cali);
        if (first_error == ESP_OK && err != ESP_OK) first_error = err;
        s_cali = NULL;
    }
    if (s_adc) {
        esp_err_t err = adc_oneshot_del_unit(s_adc);
        if (first_error == ESP_OK && err != ESP_OK) first_error = err;
        s_adc = NULL;
    }
    return first_error;
}

bool bsp_button_any_pressed(void) {
    int mv = bsp_button_read_mv();
    return mv >= 0 && mv < BSP_BTN_RELEASE_MIN_MV;
}

int bsp_button_read_mv(void) {
    // 读的是 bsp_button_init() 建好、并与 iot_button 共用的那一路 ADC。
    // 单次采样与组件的按键轮询互不干扰(oneshot 内部自带锁)。
    if (!s_adc || !s_cali) return -1;

    int raw = 0, mv = 0;
    if (adc_oneshot_read(s_adc, BSP_BTN_ADC_CHANNEL, &raw) != ESP_OK) return -1;
    if (adc_cali_raw_to_voltage(s_cali, raw, &mv) != ESP_OK) return -1;
    return mv;
}

unsigned bsp_button_sample_error_count(void) { return atomic_load(&s_sample_errors); }
