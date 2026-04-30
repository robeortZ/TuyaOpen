/**
 * @file button_driver.c
 * @brief Reusable Button Driver Implementation
 *
 * Portable GPIO button driver for T5AI platforms using TDL button management.
 * Supports 3 buttons (GPIO 44, 45, 46) with event callbacks.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tdl_button_manage.h"
#include "tdd_button_gpio.h"

#include "button_driver.h"

/* Try to include Kconfig definitions if available */
#ifdef CONFIG_ENABLE_BUTTON
    #include <tuya_kconfig.h>
#endif

/**
 * @brief Get default button GPIO configuration for standard T5AI boards
 *
 * Fills the provided config structure with default GPIO pins:
 * - Button 1: GPIO 44, active LOW
 * - Button 2: GPIO 45, active LOW
 * - Button 3: GPIO 46, active LOW
 *
 * @param[out] config Button driver configuration structure to fill
 *
 * @return OPERATE_RET_OK on success
 */
OPERATE_RET button_driver_get_default_config(button_driver_config_t *config)
{
    if (config == NULL) {
        return OPRT_INVALID_PARM;
    }

    memset(config, 0, sizeof(button_driver_config_t));

    /* Button 1 (Red) - GPIO 44 */
    config->button1_gpio.gpio_pin = TUYA_GPIO_NUM_44;
    config->button1_gpio.active_level = TUYA_GPIO_LEVEL_LOW;

    config->button1_name = "pixel_button1";


    /* Button 2 (Green) - GPIO 45 */
    config->button2_gpio.gpio_pin = TUYA_GPIO_NUM_45;
    config->button2_gpio.active_level = TUYA_GPIO_LEVEL_LOW;

    config->button2_name = "pixel_button2";


    /* Button 3 (Blue) - GPIO 46 */
    config->button3_gpio.gpio_pin = TUYA_GPIO_NUM_46;
    config->button3_gpio.active_level = TUYA_GPIO_LEVEL_LOW;

    config->button3_name = "pixel_button3";

    return OPRT_OK;
}

/**
 * @brief Register a single GPIO button at TDD layer
 *
 * Calls tdd_gpio_button_register to register a GPIO button device.
 *
 * @param[in] button_name Button name (e.g., "button1")
 * @param[in] gpio_pin GPIO pin number
 * @param[in] active_level GPIO active level (LOW/HIGH)
 *
 * @return OPERATE_RET_OK on success
 */
static OPERATE_RET __button_register_gpio(const char *button_name,
                                          TUYA_GPIO_NUM_E gpio_pin,
                                          TUYA_GPIO_LEVEL_E active_level)
{
    OPERATE_RET rt = OPRT_OK;
    BUTTON_GPIO_CFG_T button_hw_cfg;

    memset(&button_hw_cfg, 0, sizeof(BUTTON_GPIO_CFG_T));

    button_hw_cfg.pin = gpio_pin;
    button_hw_cfg.level = active_level;
    button_hw_cfg.mode = BUTTON_TIMER_SCAN_MODE;
    button_hw_cfg.pin_type.gpio_pull = TUYA_GPIO_PULLUP;

    rt = tdd_gpio_button_register((char *)button_name, &button_hw_cfg);
    if (rt != OPRT_OK) {
        PR_ERR("Failed to register GPIO button '%s' (GPIO %d): %d", button_name, gpio_pin, rt);
    }

    return rt;
}

/**
 * @brief TDL button default configuration (internal)
 *
 * @return Pre-configured TDL_BUTTON_CFG_T structure
 */
static TDL_BUTTON_CFG_T __button_get_tdl_default_config(void)
{
    TDL_BUTTON_CFG_T cfg = {
        .long_start_valid_time = 3000,      /* 3 second long press */
        .long_keep_timer = 100,             /* Trigger every 100ms during long press */
        .button_debounce_time = 20,         /* 20ms debounce */
        .button_repeat_valid_count = 2,     /* Double click requires 2 presses */
        .button_repeat_valid_time = 300,    /* 300ms window for double click */
    };
    return cfg;
}

/**
 * @brief Initialize and register GPIO buttons
 *
 * Registers 1-3 GPIO buttons by:
 * 1. Calling tdd_gpio_button_register() for GPIO registration
 * 2. Calling tdl_button_create() for TDL button management
 * 3. Registering event callbacks for press events
 *
 * @param[in] config Button configuration with GPIO pins, names, and callbacks
 * @param[out] handle Output handle containing button references
 *
 * @return OPERATE_RET_OK if successful, error code otherwise
 */
OPERATE_RET button_driver_init(const button_driver_config_t *config,
                               button_driver_handle_t *handle)
{
    OPERATE_RET rt = OPRT_OK;

    if (config == NULL || handle == NULL) {
        PR_ERR("Invalid parameters: config or handle is NULL");
        return OPRT_INVALID_PARM;
    }

    /* Get default TDL button configuration */
    TDL_BUTTON_CFG_T button_cfg = __button_get_tdl_default_config();

    /* Initialize handles to NULL */
    handle->button1 = NULL;
    handle->button2 = NULL;
    handle->button3 = NULL;

    /* ========== Button 1 Registration ========== */
    if (config->button1_name != NULL) {
        /* Step 1: Register GPIO button at TDD layer */
        rt = __button_register_gpio(config->button1_name,
                                     config->button1_gpio.gpio_pin,
                                     config->button1_gpio.active_level);
        if (rt != OPRT_OK) {
            PR_ERR("Button1 GPIO registration failed");
            return rt;
        }

        /* Step 2: Create TDL button instance */
        rt = tdl_button_create((char *)config->button1_name, &button_cfg, &handle->button1);
        if (rt != OPRT_OK) {
            PR_ERR("Failed to create button1 TDL instance: %d", rt);
            return rt;
        }

        /* Step 3: Register event callback */
        if (config->button1_callback != NULL) {
            tdl_button_event_register(handle->button1, TDL_BUTTON_PRESS_DOWN,
                                     config->button1_callback);
            PR_NOTICE("Button1 initialized: %s (GPIO %d, active %s)",
                     config->button1_name,
                     config->button1_gpio.gpio_pin,
                     config->button1_gpio.active_level == TUYA_GPIO_LEVEL_LOW ? "LOW" : "HIGH");
        }
    }

    /* ========== Button 2 Registration ========== */
    if (config->button2_name != NULL) {
        rt = __button_register_gpio(config->button2_name,
                                     config->button2_gpio.gpio_pin,
                                     config->button2_gpio.active_level);
        if (rt != OPRT_OK) {
            PR_ERR("Button2 GPIO registration failed");
            return rt;
        }

        rt = tdl_button_create((char *)config->button2_name, &button_cfg, &handle->button2);
        if (rt != OPRT_OK) {
            PR_ERR("Failed to create button2 TDL instance: %d", rt);
            return rt;
        }

        if (config->button2_callback != NULL) {
            tdl_button_event_register(handle->button2, TDL_BUTTON_PRESS_DOWN,
                                     config->button2_callback);
            PR_NOTICE("Button2 initialized: %s (GPIO %d, active %s)",
                     config->button2_name,
                     config->button2_gpio.gpio_pin,
                     config->button2_gpio.active_level == TUYA_GPIO_LEVEL_LOW ? "LOW" : "HIGH");
        }
    }

    /* ========== Button 3 Registration ========== */
    if (config->button3_name != NULL) {
        rt = __button_register_gpio(config->button3_name,
                                     config->button3_gpio.gpio_pin,
                                     config->button3_gpio.active_level);
        if (rt != OPRT_OK) {
            PR_ERR("Button3 GPIO registration failed");
            return rt;
        }

        rt = tdl_button_create((char *)config->button3_name, &button_cfg, &handle->button3);
        if (rt != OPRT_OK) {
            PR_ERR("Failed to create button3 TDL instance: %d", rt);
            return rt;
        }

        if (config->button3_callback != NULL) {
            tdl_button_event_register(handle->button3, TDL_BUTTON_PRESS_DOWN,
                                     config->button3_callback);
            PR_NOTICE("Button3 initialized: %s (GPIO %d, active %s)",
                     config->button3_name,
                     config->button3_gpio.gpio_pin,
                     config->button3_gpio.active_level == TUYA_GPIO_LEVEL_LOW ? "LOW" : "HIGH");
        }
    }

    PR_NOTICE("Button driver initialized successfully");
    return OPRT_OK;
}
