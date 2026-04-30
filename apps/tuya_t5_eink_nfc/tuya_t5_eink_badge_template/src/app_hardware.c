/**
 * @file app_hardware.c
 * @brief Hardware APIs implementation for badge template
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_hardware.h"
#include "board_com_api.h"
#include "tkl_fs.h"
#include "tal_log.h"
#include "tal_api.h"
#include "lv_vendor.h"

/***********************************************************
***********************variable define**********************
***********************************************************/
static BOOL_T  sg_sdcard_mounted       = FALSE;
static uint8_t sg_backlight_brightness = 15; // Default brightness: 15%

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET app_sdcard_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Initialize SD card hardware
    rt = board_sdcard_init();
    if (OPRT_OK != rt) {
        PR_ERR("SD card hardware initialization failed: %d", rt);
        return rt;
    }

    // Wait for SD card to be ready
    tal_system_sleep(500);

    // Mount SD card filesystem
    rt = tkl_fs_mount(SDCARD_MOUNT_PATH, DEV_SDCARD);
    if (OPRT_OK != rt) {
        PR_ERR("SD card mount failed: %d", rt);
        return rt;
    }

    sg_sdcard_mounted = TRUE;
    PR_NOTICE("SD card mounted successfully at %s", SDCARD_MOUNT_PATH);

    return OPRT_OK;
}

BOOL_T app_sdcard_is_mounted(void)
{
    return sg_sdcard_mounted;
}

const char *app_sdcard_get_mount_path(void)
{
    return SDCARD_MOUNT_PATH;
}

OPERATE_RET app_hardware_init(void)
{
    OPERATE_RET rt = OPRT_OK;

    PR_NOTICE("Initializing hardware...");

    // Register all hardware peripherals
    rt = board_register_hardware();
    if (OPRT_OK != rt) {
        PR_ERR("board_register_hardware failed: %d", rt);
        return rt;
    }

    PR_NOTICE("Hardware registration completed");

    // Wait for hardware to stabilize
    tal_system_sleep(200);

    // Initialize SD card
    rt = app_sdcard_init();
    if (OPRT_OK != rt) {
        PR_WARN("SD card initialization failed: %d (continuing)", rt);
        // Don't fail if SD card is not present
    }

    PR_NOTICE("Hardware initialization completed");

    return OPRT_OK;
}

OPERATE_RET app_backlight_set_brightness(uint8_t brightness)
{
    // Clamp brightness to valid range (0-100)
    if (brightness > 100) {
        brightness = 100;
    }

    sg_backlight_brightness = brightness;

    // Set backlight using LVGL vendor API
    // This works with the display's PWM backlight configuration
    lv_vendor_set_backlight(brightness);

    PR_NOTICE("Backlight brightness set to %d%%", brightness);

    return OPRT_OK;
}

uint8_t app_backlight_get_brightness(void)
{
    return sg_backlight_brightness;
}
