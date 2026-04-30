/**
 * @file ui_fs.h
 * @brief ui_fs module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __UI_FS_H__
#define __UI_FS_H__

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define UI_FS_FILE_LIST_MAX_SIZE 100

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    UI_FS_FILE_TYPE_DIRECTORY,
    UI_FS_FILE_TYPE_FILE_PNG,
    UI_FS_FILE_TYPE_FILE_TEXT,
    UI_FS_FILE_TYPE_FILE_EPUB,
    UI_FS_FILE_TYPE_UNKNOWN,
} ui_fs_file_type_t;

// file node
typedef struct {
    char name[UI_FS_FILE_LIST_MAX_SIZE];
    uint32_t size;
    ui_fs_file_type_t type;
    void *user_data;
    void *next;
} ui_fs_file_node_t;

// file node list
typedef struct {
    ui_fs_file_node_t *head;
    ui_fs_file_node_t *tail;
    uint32_t node_count;
} ui_fs_file_list_t;

/***********************************************************
********************function declaration********************
***********************************************************/
void ui_fs_init(void);

const char *ui_fs_file_type_to_string(ui_fs_file_type_t type);

int ui_fs_get_file_list(const char *path, ui_fs_file_list_t **file_list_p);

int ui_fs_file_list_sort(ui_fs_file_list_t *file_list);

ui_fs_file_node_t *ui_fs_file_list_get_node_by_index(ui_fs_file_list_t *file_list, uint32_t index);

int ui_fs_file_list_destroy(ui_fs_file_list_t *file_list);

int ui_fs_read_file(const char *path, uint32_t offset, void *buf, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* __UI_FS_H__ */
