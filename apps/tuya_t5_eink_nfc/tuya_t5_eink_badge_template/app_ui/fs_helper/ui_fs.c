/**
 * @file ui_fs.c
 * @brief ui_fs module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "ui.h"

#include "tal_api.h"

#include "tkl_fs.h"

#include <stdio.h>
#include <stdlib.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define UI_FS_MALLOC(size) tal_psram_malloc(size)
#define UI_FS_FREE(ptr) tal_psram_free(ptr)

#define SDCARD_MOUNT_PATH "/sdcard"

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/


/***********************************************************
***********************variable define**********************
***********************************************************/


/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Convert LVGL path to actual file system path
 * @param lvgl_path LVGL path (e.g., "/badge/xxx.png" from "S:/badge/xxx.png")
 * @param fs_path Output buffer for file system path
 * @param size Size of output buffer
 * @return 0 on success, -1 on error
 * 
 * @note LVGL removes the drive letter prefix "S:", so we receive:
 *       - Input:  "/badge/image.png"
 *       - Output: "/sdcard/badge/image.png"
 */
static int ui_fs_convert_path(const char *lvgl_path, char *fs_path, size_t size)
{
    if (lvgl_path == NULL || fs_path == NULL || size < strlen(SDCARD_MOUNT_PATH) + 2) {
        return -1;
    }
    
    // If path already starts with SDCARD_MOUNT_PATH, use it directly
    if (strncmp(lvgl_path, SDCARD_MOUNT_PATH, strlen(SDCARD_MOUNT_PATH)) == 0) {
        strncpy(fs_path, lvgl_path, size - 1);
        fs_path[size - 1] = '\0';
        return 0;
    }
    
    // If path starts with '/', prepend SDCARD_MOUNT_PATH
    if (lvgl_path[0] == '/') {
        snprintf(fs_path, size, "%s%s", SDCARD_MOUNT_PATH, lvgl_path);
        return 0;
    }
    
    // If path doesn't start with '/', prepend SDCARD_MOUNT_PATH and '/'
    snprintf(fs_path, size, "%s/%s", SDCARD_MOUNT_PATH, lvgl_path);
    return 0;
}

/**
 * @brief Check if the file system is ready
 */
static bool ui_fs_ready_cb(lv_fs_drv_t *drv)
{
    (void)drv;
    return true;
}

/**
 * @brief Open a file
 */
static void *ui_fs_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv;
    
    const char *mode_str = NULL;
    if (mode == LV_FS_MODE_RD) {
        mode_str = "rb";
    } else if (mode == LV_FS_MODE_WR) {
        mode_str = "wb";
    } else if (mode == (LV_FS_MODE_RD | LV_FS_MODE_WR)) {
        mode_str = "r+b";
    } else {
        return NULL;
    }
    
    // Convert LVGL path to actual file system path
    char fs_path[256];
    if (ui_fs_convert_path(path, fs_path, sizeof(fs_path)) != 0) {
        PR_ERR("Failed to convert path: %s", path);
        return NULL;
    }
    
    // Only log first few opens to reduce spam
    static int open_count = 0;
    if (open_count < 5) {
        PR_DEBUG("ui_fs_open_cb: LVGL[%s] -> FS[%s], mode[%d]", path, fs_path, mode);
        open_count++;
    }
    
    TUYA_FILE file = tkl_fopen(fs_path, mode_str);
    if (file == NULL) {
        PR_WARN("Failed to open file: %s", fs_path);
    }
    
    return (void *)file;
}

/**
 * @brief Close an opened file
 */
static lv_fs_res_t ui_fs_close_cb(lv_fs_drv_t *drv, void *file_p)
{
    (void)drv;
    
    TUYA_FILE file = (TUYA_FILE)file_p;
    int ret = tkl_fclose(file);
    
    return (ret == 0) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

/**
 * @brief Read data from file
 */
static lv_fs_res_t ui_fs_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    (void)drv;
    
    TUYA_FILE file = (TUYA_FILE)file_p;
    int bytes_read = tkl_fread(buf, btr, file);
    
    if (bytes_read < 0) {
        PR_ERR("ui_fs_read_cb: read failed, requested=%u, got=%d", btr, bytes_read);
        if (br) *br = 0;
        return LV_FS_RES_UNKNOWN;
    }
    
    if (br) *br = (uint32_t)bytes_read;
    return LV_FS_RES_OK;
}

/**
 * @brief Write data to file
 */
static lv_fs_res_t ui_fs_write_cb(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw)
{
    (void)drv;
    
    TUYA_FILE file = (TUYA_FILE)file_p;
    int bytes_written = tkl_fwrite((void *)buf, btw, file);
    
    if (bytes_written < 0) {
        if (bw) *bw = 0;
        return LV_FS_RES_UNKNOWN;
    }
    
    if (bw) *bw = (uint32_t)bytes_written;
    return LV_FS_RES_OK;
}

/**
 * @brief Set the file position indicator
 */
static lv_fs_res_t ui_fs_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    (void)drv;
    
    TUYA_FILE file = (TUYA_FILE)file_p;
    
    int tkl_whence;
    switch (whence) {
        case LV_FS_SEEK_SET:
            tkl_whence = SEEK_SET;
            break;
        case LV_FS_SEEK_CUR:
            tkl_whence = SEEK_CUR;
            break;
        case LV_FS_SEEK_END:
            tkl_whence = SEEK_END;
            break;
        default:
            return LV_FS_RES_INV_PARAM;
    }
    
    int ret = tkl_fseek(file, pos, tkl_whence);
    return (ret == 0) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

/**
 * @brief Get the current position in file
 */
static lv_fs_res_t ui_fs_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    (void)drv;
    
    TUYA_FILE file = (TUYA_FILE)file_p;
    int64_t pos = tkl_ftell(file);
    
    if (pos < 0) {
        return LV_FS_RES_UNKNOWN;
    }
    
    if (pos_p) *pos_p = (uint32_t)pos;
    return LV_FS_RES_OK;
}

/**
 * @brief Open a directory
 */
static void *ui_fs_dir_open_cb(lv_fs_drv_t *drv, const char *path)
{
    (void)drv;
    
    // Convert LVGL path to actual file system path
    char fs_path[256];
    if (ui_fs_convert_path(path, fs_path, sizeof(fs_path)) != 0) {
        PR_ERR("Failed to convert directory path: %s", path);
        return NULL;
    }
    
    PR_DEBUG("ui_fs_dir_open_cb: LVGL[%s] -> FS[%s]", path, fs_path);
    
    TUYA_DIR dir;
    int ret = tkl_dir_open(fs_path, &dir);
    
    if (ret != 0) {
        PR_WARN("Failed to open directory: %s", fs_path);
        return NULL;
    }
    
    return (void *)dir;
}

/**
 * @brief Read a directory
 */
static lv_fs_res_t ui_fs_dir_read_cb(lv_fs_drv_t *drv, void *rddir_p, char *fn, uint32_t fn_len)
{
    (void)drv;
    
    TUYA_DIR dir = (TUYA_DIR)rddir_p;
    TUYA_FILEINFO info;
    
    int ret = tkl_dir_read(dir, &info);
    if (ret != 0) {
        fn[0] = '\0';
        return LV_FS_RES_OK; // End of directory
    }
    
    const char *name = NULL;
    ret = tkl_dir_name(info, &name);
    if (ret != 0 || name == NULL) {
        fn[0] = '\0';
        return LV_FS_RES_UNKNOWN;
    }
    
    // Check if it's a directory and prepend '/' if so
    BOOL_T is_dir = FALSE;
    tkl_dir_is_directory(info, &is_dir);
    
    if (is_dir) {
        if (fn_len < 2) {
            fn[0] = '\0';
            return LV_FS_RES_UNKNOWN;
        }
        fn[0] = '/';
        strncpy(fn + 1, name, fn_len - 2);
        fn[fn_len - 1] = '\0';
    } else {
        strncpy(fn, name, fn_len - 1);
        fn[fn_len - 1] = '\0';
    }
    
    return LV_FS_RES_OK;
}

/**
 * @brief Close a directory
 */
static lv_fs_res_t ui_fs_dir_close_cb(lv_fs_drv_t *drv, void *rddir_p)
{
    (void)drv;
    
    TUYA_DIR dir = (TUYA_DIR)rddir_p;
    int ret = tkl_dir_close(dir);
    
    return (ret == 0) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

/**
 * @brief Initialize LVGL file system driver
 */
void ui_fs_init(void)
{
    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);
    
    // Set driver letter (can be any letter, using 'S' for Sdcard/Storage)
    drv.letter = 'S';
    
    // Set cache size (0 means no cache)
    drv.cache_size = 0;
    
    // Register callback functions
    drv.ready_cb = ui_fs_ready_cb;
    drv.open_cb = ui_fs_open_cb;
    drv.close_cb = ui_fs_close_cb;
    drv.read_cb = ui_fs_read_cb;
    drv.write_cb = ui_fs_write_cb;
    drv.seek_cb = ui_fs_seek_cb;
    drv.tell_cb = ui_fs_tell_cb;
    drv.dir_open_cb = ui_fs_dir_open_cb;
    drv.dir_read_cb = ui_fs_dir_read_cb;
    drv.dir_close_cb = ui_fs_dir_close_cb;
    
    // Register the driver
    lv_fs_drv_register(&drv);
    
    PR_DEBUG("==================================================");
    PR_DEBUG("LVGL file system driver registered successfully!");
    PR_DEBUG("  Driver letter: 'S'");
    PR_DEBUG("  Mount point: %s", SDCARD_MOUNT_PATH);
    PR_DEBUG("  Path mapping:");
    PR_DEBUG("    LVGL path: S:/badge/image.png");
    PR_DEBUG("    Real path: %s/badge/image.png", SDCARD_MOUNT_PATH);
    PR_DEBUG("  Usage example:");
    PR_DEBUG("    lv_image_set_src(img, \"S:/badge/image.png\");");
    PR_DEBUG("==================================================");
}


const char *ui_fs_file_type_to_string(ui_fs_file_type_t type)
{
    switch (type) {
        case UI_FS_FILE_TYPE_DIRECTORY:
            return "DIRECTORY";
        case UI_FS_FILE_TYPE_FILE_TEXT:
            return "FILE_TEXT";
        case UI_FS_FILE_TYPE_FILE_PNG:
            return "FILE_PNG";
        case UI_FS_FILE_TYPE_FILE_EPUB:
            return "FILE_EPUB";
        case UI_FS_FILE_TYPE_UNKNOWN:
            return "UNKNOWN";
        default:
            return "UNKNOWN";
    }
}

int ui_fs_file_list_destroy(ui_fs_file_list_t *file_list)
{
    if (file_list == NULL) {
        return 0;
    }

    ui_fs_file_node_t *node = file_list->head;
    while (node != NULL) {
        ui_fs_file_node_t *next = node->next;
        UI_FS_FREE(node);
        node = next;
    }

    UI_FS_FREE(file_list);

    return 0;
}

// get file list
int ui_fs_get_file_list(const char *path, ui_fs_file_list_t **file_list_p)
{
    ui_fs_file_list_t *file_list = NULL;
    ui_fs_file_node_t *node = NULL;

    PR_DEBUG("ui_fs_get_file_list path: [%s]", path);

    if (path == NULL || file_list_p == NULL) {
        return -1;
    }

    TUYA_DIR dir;
    int rt = tkl_dir_open(path, &dir);
    if (rt != 0 || dir == NULL) {
        return -1;
    }

    file_list = UI_FS_MALLOC(sizeof(ui_fs_file_list_t));
    if (file_list == NULL) {
        tkl_dir_close(dir);
        return -1;
    }
    file_list->head = NULL;
    file_list->tail = NULL;
    file_list->node_count = 0;

    TUYA_FILEINFO info;
    while (tkl_dir_read(dir, &info) == 0) {
        const char *name = NULL;
        rt = tkl_dir_name(info, &name);
        if (rt != 0 || name == NULL) {
            continue;
        }

        // Skip . and .. entries
        if (name[0] == '.') {
            continue;
        }

        PR_DEBUG("ui_fs_get_file_list name: [%s]", name);

        node = UI_FS_MALLOC(sizeof(ui_fs_file_node_t));
        if (node == NULL) {
            goto __ERR;
        }
        memset(node->name, 0, UI_FS_FILE_LIST_MAX_SIZE);
        strncpy(node->name, name, UI_FS_FILE_LIST_MAX_SIZE - 1);
        node->name[UI_FS_FILE_LIST_MAX_SIZE - 1] = '\0';

        // set file type
        BOOL_T is_dir = FALSE;
        if (tkl_dir_is_directory(info, &is_dir) == 0 && is_dir) {
            node->type = UI_FS_FILE_TYPE_DIRECTORY;
        } else {
            // Check file extension (from the end of the filename)
            size_t name_len = strlen(name);
            if (name_len > 4 && strncmp(name + name_len - 4, ".txt", 4) == 0) {
                node->type = UI_FS_FILE_TYPE_FILE_TEXT;
            } else if (name_len > 4 && strncmp(name + name_len - 4, ".png", 4) == 0) {
                node->type = UI_FS_FILE_TYPE_FILE_PNG;
            } else if (name_len > 5 && strncmp(name + name_len - 5, ".epub", 5) == 0) {
                node->type = UI_FS_FILE_TYPE_FILE_EPUB;
            } else {
                node->type = UI_FS_FILE_TYPE_UNKNOWN;
            }
        }

        // set file size
        if (is_dir) {
            node->size = 0;
        } else {
            // Build full file path to get size
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, name);
            int file_size = tkl_fgetsize(full_path);
            node->size = (file_size >= 0) ? file_size : 0;
        }

        // set user data
        node->user_data = NULL;

        // set next node
        node->next = NULL;

        // add node to file list
        if (file_list->head == NULL) {
            file_list->head = node;
        } else {
            file_list->tail->next = node;
        }
        file_list->tail = node;
        file_list->node_count++;
    }

    tkl_dir_close(dir);
    *file_list_p = file_list;

    // DEBUG
    PR_DEBUG("--- file list count: [%d] ---", file_list->node_count);
    node = file_list->head;
    while (node != NULL) {
        // Format file size for better readability
        char size_str[32];
        if (node->type == UI_FS_FILE_TYPE_DIRECTORY) {
            snprintf(size_str, sizeof(size_str), "-");
        } else if (node->size < 1024) {
            snprintf(size_str, sizeof(size_str), "%u B", node->size);
        } else if (node->size < 1024 * 1024) {
            snprintf(size_str, sizeof(size_str), "%.2f KB", node->size / 1024.0);
        } else {
            snprintf(size_str, sizeof(size_str), "%.2f MB", node->size / (1024.0 * 1024.0));
        }

        PR_DEBUG("file name: [%s], file size: [%s], file type: [%s]",
                    node->name, size_str, ui_fs_file_type_to_string(node->type));
        node = node->next;
    }

    return 0;

__ERR:
    ui_fs_file_list_destroy(file_list);

    return -1;
}

/**
 * @brief Sort file list by type first, then by name
 *
 * Sorting rules:
 * 1. Sort by file type (DIRECTORY < FILE_PNG < FILE_TEXT < FILE_EPUB < UNKNOWN)
 * 2. When file type is the same, sort by file name (case-insensitive alphabetical order)
 *
 * @param file_list Pointer to the file list to be sorted
 * @return 0 on success, -1 on failure
 */
int ui_fs_file_list_sort(ui_fs_file_list_t *file_list)
{
    if (file_list == NULL) {
        return -1;
    }

    // Nothing to sort if less than 2 nodes
    if (file_list->node_count < 2) {
        return 0;
    }

    // Use insertion sort algorithm to sort the linked list
    ui_fs_file_node_t *sorted = NULL;  // Head of sorted list
    ui_fs_file_node_t *node = file_list->head;

    while (node != NULL) {
        ui_fs_file_node_t *next = (ui_fs_file_node_t *)node->next;
        node->next = NULL;

        // Insert node into sorted list
        if (sorted == NULL) {
            // First node in sorted list
            sorted = node;
        } else {
            ui_fs_file_node_t *prev = NULL;
            ui_fs_file_node_t *curr = sorted;

            // Find the correct position to insert
            while (curr != NULL) {
                // Compare by file type first
                if (node->type < curr->type) {
                    break;
                } else if (node->type == curr->type) {
                    // Same type, compare by name (case-insensitive)
                    int cmp_result = strcasecmp(node->name, curr->name);
                    if (cmp_result < 0) {
                        break;
                    } else if (cmp_result == 0) {
                        // Names are equal (case-insensitive), use case-sensitive comparison for stability
                        if (strcmp(node->name, curr->name) < 0) {
                            break;
                        }
                    }
                }
                prev = curr;
                curr = (ui_fs_file_node_t *)curr->next;
            }

            if (prev == NULL) {
                // Insert at the beginning
                node->next = sorted;
                sorted = node;
            } else {
                // Insert after prev
                node->next = prev->next;
                prev->next = node;
            }
        }

        node = next;
    }

    // Update the file list head and tail
    file_list->head = sorted;

    // Find the tail by traversing to the last node
    ui_fs_file_node_t *tail = sorted;
    while (tail != NULL && tail->next != NULL) {
        tail = (ui_fs_file_node_t *)tail->next;
    }
    file_list->tail = tail;

    return 0;
}

ui_fs_file_node_t *ui_fs_file_list_get_node_by_index(ui_fs_file_list_t *file_list, uint32_t index)
{
    if (file_list == NULL || index >= file_list->node_count) {
        return NULL;
    }

    ui_fs_file_node_t *node = file_list->head;
    for (uint32_t i = 0; i < index; i++) {
        node = node->next;
    }
    return node;
}

int ui_fs_read_file(const char *path, uint32_t offset, void *buf, uint32_t size)
{
    (void) offset;

    if (path == NULL || buf == NULL || size == 0) {
        return -1;
    }

    return 0;

// __ERR:
//     return -1;
}
