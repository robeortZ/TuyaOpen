/**
 * @file app_hardware.h
 * @brief Hardware APIs for badge template
 *
 * This module provides simple APIs for hardware initialization and control:
 * - SD card initialization and mounting
 * - Button management
 * - LED control
 * - Display initialization
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef APP_HARDWARE_H
#define APP_HARDWARE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define SDCARD_MOUNT_PATH "/sdcard"

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize all hardware peripherals
 *
 * This function initializes:
 * - Hardware registration (buttons, LED, display, SD card)
 * - SD card mounting
 *
 * @return OPERATE_RET_OK on success, error code otherwise
 */
OPERATE_RET app_hardware_init(void);

/**
 * @brief Initialize SD card hardware and mount filesystem
 *
 * @return OPERATE_RET_OK on success, error code otherwise
 */
OPERATE_RET app_sdcard_init(void);

/**
 * @brief Check if SD card is mounted
 *
 * @return TRUE if mounted, FALSE otherwise
 */
BOOL_T app_sdcard_is_mounted(void);

/**
 * @brief Get SD card mount path
 *
 * @return Mount path string (e.g., "/sdcard")
 */
const char *app_sdcard_get_mount_path(void);

/**
 * @brief Set e-ink display backlight brightness
 *
 * @param brightness Brightness level (0-100, where 0 is off and 100 is maximum)
 * @return OPERATE_RET_OK on success, error code otherwise
 */
OPERATE_RET app_backlight_set_brightness(uint8_t brightness);

/**
 * @brief Get current backlight brightness
 *
 * @return Current brightness level (0-100)
 */
uint8_t app_backlight_get_brightness(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_HARDWARE_H */
