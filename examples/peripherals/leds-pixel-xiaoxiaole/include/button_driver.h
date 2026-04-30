/**
 * @file button_driver.h
 * @brief Reusable Button Driver Module for T5AI Boards
 *
 * This module provides a portable interface for GPIO button management
 * using TDL (Tuya Device Layer) button API with support for:
 * - Debouncing (20ms)
 * - Long press detection (3000ms)
 * - Double-click detection (300ms window)
 * - Press-down event handling
 *
 * Supports up to 3 buttons with custom event callbacks.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BUTTON_DRIVER_H__
#define __BUTTON_DRIVER_H__

#include "tuya_cloud_types.h"
#include "tdl_button_manage.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Button event callback type
 *
 * Called when a registered button is pressed.
 *
 * @param[in] name Button name (from BUTTON_NAME macro)
 * @param[in] event Button event type
 * @param[in] argc Reserved parameter
 */
typedef void (*button_event_callback_t)(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);

/**
 * @brief Button number definitions
 */
typedef enum {
    BUTTON_NUM_1 = 0,  /* Red button - GPIO 44 */
    BUTTON_NUM_2 = 1,  /* Green button - GPIO 45 */
    BUTTON_NUM_3 = 2,  /* Blue button - GPIO 46 */
    BUTTON_NUM_MAX = 3,
} button_num_e;

/**
 * @brief GPIO Button configuration structure (for single button GPIO registration)
 *
 * Holds GPIO PIN number and active level for a button.
 */
typedef struct {
    TUYA_GPIO_NUM_E gpio_pin;              /* GPIO pin number (e.g., TUYA_GPIO_NUM_44) */
    TUYA_GPIO_LEVEL_E active_level;        /* Active level: TUYA_GPIO_LEVEL_LOW or HIGH */
} button_gpio_config_t;

/**
 * @brief Button configuration structure
 *
 * Defines which buttons to initialize with GPIO pins and their callbacks.
 */
typedef struct {
    /* Button 1 (GPIO 44 - Red) */
    button_gpio_config_t button1_gpio;          /* GPIO pin and active level */
    char *button1_name;                         /* e.g., "button1", or NULL to skip */
    button_event_callback_t button1_callback;   /* Callback for button1, or NULL */

    /* Button 2 (GPIO 45 - Green) */
    button_gpio_config_t button2_gpio;          /* GPIO pin and active level */
    char *button2_name;                         /* e.g., "button2", or NULL to skip */
    button_event_callback_t button2_callback;   /* Callback for button2, or NULL */

    /* Button 3 (GPIO 46 - Blue) */
    button_gpio_config_t button3_gpio;          /* GPIO pin and active level */
    char *button3_name;                         /* e.g., "button3", or NULL to skip */
    button_event_callback_t button3_callback;   /* Callback for button3, or NULL */
} button_driver_config_t;

/**
 * @brief Button handle structure (opaque to user)
 *
 * Contains internal state for registered buttons.
 */
typedef struct {
    TDL_BUTTON_HANDLE button1;
    TDL_BUTTON_HANDLE button2;
    TDL_BUTTON_HANDLE button3;
} button_driver_handle_t;

/**
 * @brief Get default button GPIO configuration for standard T5AI boards
 *
 * Returns GPIO configuration for standard T5AI Pixel board:
 * - Button 1: GPIO 44, active LOW
 * - Button 2: GPIO 45, active LOW
 * - Button 3: GPIO 46, active LOW
 *
 * @param[out] config Button driver configuration structure (pre-filled)
 *
 * @return OPERATE_RET_OK on success
 */
OPERATE_RET button_driver_get_default_config(button_driver_config_t *config);

/**
 * @brief Initialize and register GPIO buttons
 *
 * Registers 1-3 GPIO buttons with direct GPIO registration (tdd_gpio_button_register)
 * and TDL button management. Each button can have a custom event callback or be
 * skipped (set name to NULL).
 *
 * This function is fully portable - no board-level configuration required.
 * It directly registers GPIO pins and manages button events.
 *
 * @param[in] config Button configuration with GPIO pins, names, and callbacks
 * @param[out] handle Output handle containing button references
 *
 * @return OPERATE_RET_OK on success, error code on failure
 *
 * Example:
 * @code
 * button_driver_handle_t btn_handle;
 * button_driver_config_t btn_cfg;
 * button_driver_get_default_config(&btn_cfg);  // Use default GPIO pins
 * btn_cfg.button1_callback = my_red_button_handler;
 * btn_cfg.button2_callback = my_green_button_handler;
 * btn_cfg.button3_callback = my_blue_button_handler;
 * button_driver_init(&btn_cfg, &btn_handle);
 * @endcode
 */
OPERATE_RET button_driver_init(const button_driver_config_t *config,
                               button_driver_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif // __BUTTON_DRIVER_H__
