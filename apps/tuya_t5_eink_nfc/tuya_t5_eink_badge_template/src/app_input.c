/**
 * @file app_input.c
 * @brief Input device implementation for LVGL keypad
 *
 * Maps 6 buttons (UP, DOWN, LEFT, RIGHT, ENTER, RETURN) to LVGL keypad input device
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_input.h"
#include "tdl_button_manage.h"
#include "board_com_api.h"
#include "tal_log.h"
#include <string.h>
#include <stdbool.h>

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    char    *button_name;
    uint32_t lv_key;
} BUTTON_KEY_MAP_T;

/***********************************************************
***********************variable define**********************
***********************************************************/
static lv_indev_t *sg_keypad_indev                                    = NULL;
static uint32_t    sg_current_key                                     = 0;
static void (*sg_status_callback)(const char *key_name, bool pressed) = NULL;

// Button to LVGL key mapping
static const BUTTON_KEY_MAP_T button_key_map[] = {
    {BOARD_BUTTON_NAME_UP, LV_KEY_UP},       {BOARD_BUTTON_NAME_DOWN, LV_KEY_DOWN},
    {BOARD_BUTTON_NAME_LEFT, LV_KEY_LEFT},   {BOARD_BUTTON_NAME_RIGHT, LV_KEY_RIGHT},
    {BOARD_BUTTON_NAME_ENTER, LV_KEY_ENTER}, {BOARD_BUTTON_NAME_RETURN, LV_KEY_ESC},
};

/***********************************************************
********************function declaration********************
***********************************************************/
static void button_callback(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Button event callback - maps button events to LVGL keys
 */
static void button_callback(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    static uint32_t g_button_press_cnt = 0;

    (void)argc;
    uint32_t    i;
    const char *key_name = "UNKNOWN";

    // Handle button press and release events
    if (event == TDL_BUTTON_PRESS_DOWN) {
        // Find matching button and set corresponding LVGL key
        for (i = 0; i < sizeof(button_key_map) / sizeof(button_key_map[0]); i++) {
            if (strcmp(name, button_key_map[i].button_name) == 0) {
                sg_current_key = button_key_map[i].lv_key;

                // Map button name to display name
                if (strcmp(name, BOARD_BUTTON_NAME_UP) == 0) {
                    key_name = "UP";
                } else if (strcmp(name, BOARD_BUTTON_NAME_DOWN) == 0) {
                    key_name = "DOWN";
                } else if (strcmp(name, BOARD_BUTTON_NAME_LEFT) == 0) {
                    key_name = "LEFT";
                } else if (strcmp(name, BOARD_BUTTON_NAME_RIGHT) == 0) {
                    key_name = "RIGHT";
                } else if (strcmp(name, BOARD_BUTTON_NAME_ENTER) == 0) {
                    key_name = "ENTER";
                } else if (strcmp(name, BOARD_BUTTON_NAME_RETURN) == 0) {
                    key_name = "RETURN";
                }

                // Notify status callback
                if (sg_status_callback != NULL) {
                    sg_status_callback(key_name, true);
                }

                PR_DEBUG("Button '%s' pressed -> LV_KEY: %lu", name, sg_current_key);
                g_button_press_cnt++;
                if (g_button_press_cnt >= 10) {
                    g_button_press_cnt = 0;
                    extern void __set_need_partial_refresh(bool need_partial_refresh);
                    __set_need_partial_refresh(false);
                }
                break;
            }
        }
    } else if (event == TDL_BUTTON_PRESS_UP) {
        // Button released
        for (i = 0; i < sizeof(button_key_map) / sizeof(button_key_map[0]); i++) {
            if (strcmp(name, button_key_map[i].button_name) == 0) {
                // Map button name to display name
                if (strcmp(name, BOARD_BUTTON_NAME_UP) == 0) {
                    key_name = "UP";
                } else if (strcmp(name, BOARD_BUTTON_NAME_DOWN) == 0) {
                    key_name = "DOWN";
                } else if (strcmp(name, BOARD_BUTTON_NAME_LEFT) == 0) {
                    key_name = "LEFT";
                } else if (strcmp(name, BOARD_BUTTON_NAME_RIGHT) == 0) {
                    key_name = "RIGHT";
                } else if (strcmp(name, BOARD_BUTTON_NAME_ENTER) == 0) {
                    key_name = "ENTER";
                } else if (strcmp(name, BOARD_BUTTON_NAME_RETURN) == 0) {
                    key_name = "RETURN";
                }

                // Notify status callback
                if (sg_status_callback != NULL) {
                    sg_status_callback(key_name, false);
                }

                PR_DEBUG("Button '%s' released", name);
                break;
            }
        }
    }
}

/**
 * @brief LVGL keypad read callback (LVGL v9 API)
 *
 * This callback is called by LVGL to read keypad input.
 * It checks if a key was pressed (cur_key != 0) and reports it.
 */
static void keypad_read_cb(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    (void)indev_drv;

    // Set the key
    data->key = sg_current_key;

    // Set the key state - if there's a key press, set to PRESSED, otherwise RELEASED
    if (sg_current_key != 0) {
        data->state    = LV_INDEV_STATE_PRESSED;
        sg_current_key = 0; // Clear the key after reading to avoid repeated events
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void *app_input_get_keypad_indev(void)
{
    return (void *)sg_keypad_indev;
}

void app_input_set_status_callback(void (*callback)(const char *key_name, bool pressed))
{
    sg_status_callback = callback;
}

OPERATE_RET app_input_init(void)
{
    OPERATE_RET       rt            = OPRT_OK;
    TDL_BUTTON_CFG_T  button_cfg    = {.long_start_valid_time     = 2000,
                                       .long_keep_timer           = 500,
                                       .button_debounce_time      = 50,
                                       .button_repeat_valid_count = 0,
                                       .button_repeat_valid_time  = 500};
    TDL_BUTTON_HANDLE button_handle = NULL;
    uint32_t          i;

    PR_NOTICE("Initializing button input devices...");

    // Create and register all buttons
    for (i = 0; i < sizeof(button_key_map) / sizeof(button_key_map[0]); i++) {
        rt = tdl_button_create(button_key_map[i].button_name, &button_cfg, &button_handle);
        if (OPRT_OK != rt) {
            PR_ERR("Failed to create button '%s': %d", button_key_map[i].button_name, rt);
            continue;
        }

        // Register button event callbacks for press and release
        tdl_button_event_register(button_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, button_callback);
        tdl_button_event_register(button_handle, TDL_BUTTON_PRESS_DOWN, button_callback);
        tdl_button_event_register(button_handle, TDL_BUTTON_PRESS_UP, button_callback);

        PR_NOTICE("Button '%s' registered (LV_KEY: %lu)", button_key_map[i].button_name, button_key_map[i].lv_key);
    }

    // Register LVGL keypad input device (LVGL v9 API)
    sg_keypad_indev = lv_indev_create();
    if (sg_keypad_indev == NULL) {
        PR_ERR("Failed to create LVGL keypad input device");
        return OPRT_MALLOC_FAILED;
    }

    lv_indev_set_type(sg_keypad_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(sg_keypad_indev, keypad_read_cb);

    // Create and set default group for keypad navigation
    lv_group_t *group = lv_group_get_default();
    if (group == NULL) {
        group = lv_group_create();
        lv_group_set_default(group);
    }
    lv_indev_set_group(sg_keypad_indev, group);

    PR_NOTICE("LVGL keypad input device registered successfully");

    return OPRT_OK;
}
