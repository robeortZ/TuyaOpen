/**
 * @file app_lvgl.h
 * @brief LVGL initialization and management APIs
 *
 * This module provides simple APIs for LVGL initialization:
 * - Display initialization
 * - LVGL initialization
 * - Basic display setup
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef APP_LVGL_H
#define APP_LVGL_H

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
 * @brief Initialize LVGL display and graphics system
 *
 * This function:
 * - Opens the display device
 * - Initializes LVGL
 * - Sets up the display buffer
 *
 * @return OPERATE_RET_OK on success, error code otherwise
 */
OPERATE_RET app_lvgl_init(void);

/**
 * @brief Get LVGL display handle
 *
 * @return Display handle, or NULL if not initialized
 */
void *app_lvgl_get_display(void);

/**
 * @brief Start LVGL task handler
 *
 * This should be called after all initialization is complete, including input device registration.
 *
 * @param task_priority LVGL task priority (default: 4)
 * @param stack_size LVGL task stack size (default: 4096)
 */
void app_lvgl_start(uint32_t task_priority, uint32_t stack_size);

#ifdef __cplusplus
}
#endif

#endif /* APP_LVGL_H */
