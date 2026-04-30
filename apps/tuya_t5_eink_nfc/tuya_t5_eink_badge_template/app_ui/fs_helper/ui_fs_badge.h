/**
 * @file ui_fs_badge.h
 * @brief ui_fs_badge module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __UI_FS_BADGE_H__
#define __UI_FS_BADGE_H__

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define UI_FS_BADGE_MAX_BADGE_COUNT 10
#define UI_FS_BADGE_STRING_MAX_LEN 256

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    UI_FS_BADGE_TYPE_BADGE = 0,
    UI_FS_BADGE_TYPE_BADGE_IMAGE,
} ui_fs_badge_type_t;

typedef struct {
    uint8_t id;
    ui_fs_badge_type_t type;
    
    // For type "badge"
    char firstname[UI_FS_BADGE_STRING_MAX_LEN];
    char lastname[UI_FS_BADGE_STRING_MAX_LEN];
    char title[UI_FS_BADGE_STRING_MAX_LEN];
    char email[UI_FS_BADGE_STRING_MAX_LEN];
    char phone[UI_FS_BADGE_STRING_MAX_LEN];
    char site[UI_FS_BADGE_STRING_MAX_LEN];
    char company[UI_FS_BADGE_STRING_MAX_LEN];
    char profile_picture[UI_FS_BADGE_STRING_MAX_LEN];
    char slogan[UI_FS_BADGE_STRING_MAX_LEN];
    char image_QR[UI_FS_BADGE_STRING_MAX_LEN];
    
    // For type "badge_image"
    char image_path[UI_FS_BADGE_STRING_MAX_LEN];
    char nfc_data_type[UI_FS_BADGE_STRING_MAX_LEN];
    char nfc_data[UI_FS_BADGE_STRING_MAX_LEN];
} ui_fs_badge_item_t;

typedef struct {
    uint8_t default_badge_id;
    ui_fs_badge_item_t badge_list[UI_FS_BADGE_MAX_BADGE_COUNT];
    uint32_t badge_count;
} ui_fs_badge_data_t;

/***********************************************************
********************function declaration********************
***********************************************************/
int ui_fs_badge_init(const char *json_path);
void ui_fs_badge_deinit(void);
int ui_fs_badge_read(uint8_t id, ui_fs_badge_item_t *badge_item);
uint32_t ui_fs_badge_list_number_get(void);


#ifdef __cplusplus
}
#endif

#endif /* __UI_FS_BADGE_H__ */
