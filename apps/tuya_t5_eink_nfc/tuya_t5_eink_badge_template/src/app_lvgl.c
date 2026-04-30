/**
 * @file app_lvgl.c
 * @brief LVGL initialization implementation
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_lvgl.h"
#include "app_hardware.h"
#include "tdl_display_manage.h"
#include "tal_log.h"
#include "tal_api.h"
#include "lv_vendor.h"

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/***********************************************************
***********************variable define**********************
***********************************************************/
static TDL_DISP_HANDLE_T sg_disp_handle = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

void *app_lvgl_get_display(void)
{
    return (void *)sg_disp_handle;
}

OPERATE_RET app_lvgl_init(void)
{
    const char *display_name = "display";

    PR_NOTICE("Initializing LVGL...");

    // Find display device (for reference, lv_vendor_init will also handle this)
    sg_disp_handle = tdl_disp_find_dev((char *)display_name);
    if (sg_disp_handle == NULL) {
        PR_ERR("Display '%s' not found", display_name);
        return OPRT_NOT_FOUND;
    }

    // Initialize LVGL vendor (this initializes LVGL, display port, and input port)
    // Note: lv_vendor_init will call lv_port_disp_init and lv_port_indev_init
    // We'll register our keypad input device separately in app_input_init()
    lv_vendor_init((void *)display_name);

    PR_NOTICE("LVGL vendor initialized");

    // Wait for display to be ready
    tal_system_sleep(100);

    // Set default backlight brightness to 15%
    app_backlight_set_brightness(15);

    PR_NOTICE("LVGL initialization completed");

    return OPRT_OK;
}

/**
 * @brief Start LVGL task handler
 *
 * This should be called after all initialization is complete, including input device registration.
 *
 * @param task_priority LVGL task priority (default: 4)
 * @param stack_size LVGL task stack size (default: 4096)
 */
void app_lvgl_start(uint32_t task_priority, uint32_t stack_size)
{
    lv_vendor_start(task_priority, stack_size);
    PR_NOTICE("LVGL task started");
}

/* Stub: e-ink partial refresh control (called from app_input and badge_screen) */
void __set_need_partial_refresh(bool need_partial_refresh)
{
    (void)need_partial_refresh;
}
