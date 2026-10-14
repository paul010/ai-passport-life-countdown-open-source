#ifndef LQ_NATIVE_LV_CONF_H
#define LQ_NATIVE_LV_CONF_H
#define LV_CONF_H
#ifndef __ASSEMBLER__
#include <stdlib.h>
#endif
#define LV_ASSERT_HANDLER abort();
#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_NONE
/* Native LVGL objects have 64-bit pointers; the device uses 32-bit pointers.
 * This host-only allowance is not a claim about the device's 32 KiB pool.
 * Device diagnostics separately verify actual free heap and LVGL memory. */
#define LV_MEM_SIZE (48U * 1024U)
#define LV_USE_SNAPSHOT 1
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_USE_THORVG_INTERNAL 0
#define LV_USE_THORVG_EXTERNAL 0
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_OBJ 1
#endif
