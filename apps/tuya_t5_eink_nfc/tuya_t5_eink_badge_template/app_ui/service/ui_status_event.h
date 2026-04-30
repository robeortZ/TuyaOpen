/**
 * @file ui_status_event.h
 * @brief ui_status_event module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __UI_STATUS_EVENT_H__
#define __UI_STATUS_EVENT_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    uint8_t charging;
    uint8_t battery_level;
} ui_status_event_battery_t;

/***********************************************************
***********************variable define**********************
***********************************************************/
extern uint32_t LV_EVENT_APP_BATTERY;

/***********************************************************
********************function declaration********************
***********************************************************/
void ui_status_event_init(void);
void ui_status_event_deinit(void);

// battery service
// send event
void ui_status_event_send_battery_event(uint8_t charging, uint8_t battery_level);

// battery service handler
void ui_battery_event_handler(lv_obj_t * obj);

#ifdef __cplusplus
}
#endif

#endif /* __UI_STATUS_EVENT_H__ */
