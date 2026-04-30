/**
 * @file ui_status_event.c
 * @brief ui_status_event module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

 #include "ui.h"
#include "ui_status_event.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
uint32_t LV_EVENT_APP_BATTERY = 0;

static ui_status_event_battery_t __battery_status = {
    .charging = 0,
    .battery_level = 100,
};

/***********************************************************
***********************function define**********************
***********************************************************/

void ui_status_event_init(void)
{
    if (LV_EVENT_APP_BATTERY == 0) {
        LV_EVENT_APP_BATTERY = lv_event_register_id();
    }

    return;
}

void ui_status_event_deinit(void)
{
    return;
}

/************************* battery service start ************************** */

void ui_status_event_send_battery_event(uint8_t charging, uint8_t battery_level)
{
    __battery_status.charging = charging;
    __battery_status.battery_level = battery_level;

    if (LV_EVENT_APP_BATTERY == 0) {
        return;
    }

    // get current screen
    lv_obj_t * current_screen = lv_scr_act();
    if (current_screen == NULL) {
        return;
    }

    // send event to current screen
    lv_obj_send_event(current_screen, LV_EVENT_APP_BATTERY, NULL);
}

void ui_battery_event_handler(lv_obj_t * obj)
{
    if (obj == NULL) {
        return;
    }

    if (__battery_status.charging) {
        lv_image_set_src(obj, &ui_img_image_statue_bar_battery_battery_charging_30_png);
        return;
    }

    if (__battery_status.battery_level >= 75) {
        lv_image_set_src(obj, &ui_img_image_statue_bar_battery_battery_full_30_png);
    } else if (__battery_status.battery_level >= 20) {
        lv_image_set_src(obj, &ui_img_image_statue_bar_battery_battery_60_30_png);
    } else if (__battery_status.battery_level >= 10) {
        lv_image_set_src(obj, &ui_img_image_statue_bar_battery_battery_low_30_png);
    } else {
        lv_image_set_src(obj, &ui_img_image_statue_bar_battery_battery_empty_30_png);
    }
}

/************************* battery service end ************************** */