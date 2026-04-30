/**
 * @file app_input.h
 * @brief Input device APIs for LVGL keypad input
 *
 * This module provides APIs for button input as LVGL keypad:
 * - Button to keypad mapping (UP, DOWN, LEFT, RIGHT, ENTER, RETURN)
 * - LVGL keypad input device registration
 * - Button event handling
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef APP_INPUT_H
#define APP_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Initialize button input devices and register as LVGL keypad
 *
 * This function:
 * - Creates button handlers for all 6 buttons (UP, DOWN, LEFT, RIGHT, ENTER, RETURN)
 * - Registers buttons as LVGL keypad input device
 * - Maps button events to LVGL key codes
 *
 * @return OPERATE_RET_OK on success, error code otherwise
 */
OPERATE_RET app_input_init(void);

/**
 * @brief Get LVGL keypad input device handle
 *
 * @return Input device handle, or NULL if not initialized
 */
void *app_input_get_keypad_indev(void);

/**
 * @brief Register callback for key status updates
 *
 * This allows the input module to notify the application when keys are pressed/released.
 * The callback will be called with the key name and pressed state.
 *
 * @param callback Function pointer: void callback(const char *key_name, bool pressed)
 */
void app_input_set_status_callback(void (*callback)(const char *key_name, bool pressed));

#ifdef __cplusplus
}
#endif

#endif /* APP_INPUT_H */
