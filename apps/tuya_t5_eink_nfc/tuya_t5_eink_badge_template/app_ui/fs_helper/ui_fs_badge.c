/**
 * @file ui_fs_badge.c
 * @brief ui_fs_badge module is used to 
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "ui.h"

#include "ui_fs.h"
#include "ui_fs_badge.h"
#include "cJSON.h"

#include "tal_api.h"
#include "tkl_fs.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define LOCAL_MALLOC(size) tal_psram_malloc(size)
#define LOCAL_FREE(ptr) tal_psram_free(ptr)

/***********************************************************
***********************typedef define***********************
***********************************************************/


/***********************************************************
********************function declaration********************
***********************************************************/
static int parse_badge_json(const char *json_string);
static void parse_badge_item(cJSON *item_json, ui_fs_badge_item_t *badge_item);
static void safe_strcpy(char *dest, const char *src, size_t dest_size);

/***********************************************************
***********************variable define**********************
***********************************************************/
static ui_fs_badge_data_t *g_badge_data = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

static void safe_strcpy(char *dest, const char *src, size_t dest_size)
{
    if (dest == NULL || dest_size == 0) {
        return;
    }
    
    if (src == NULL) {
        dest[0] = '\0';
        return;
    }
    
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

static void parse_badge_item(cJSON *item_json, ui_fs_badge_item_t *badge_item)
{
    if (item_json == NULL || badge_item == NULL) {
        return;
    }
    
    memset(badge_item, 0, sizeof(ui_fs_badge_item_t));
    
    // Parse common fields
    cJSON *id_item = cJSON_GetObjectItem(item_json, "id");
    if (cJSON_IsNumber(id_item)) {
        badge_item->id = (uint8_t)cJSON_GetNumberValue(id_item);
    }
    
    cJSON *type_item = cJSON_GetObjectItem(item_json, "type");
    if (cJSON_IsString(type_item)) {
        const char *type_str = cJSON_GetStringValue(type_item);
        if (strcmp(type_str, "badge") == 0) {
            badge_item->type = UI_FS_BADGE_TYPE_BADGE;
        } else if (strcmp(type_str, "badge_image") == 0) {
            badge_item->type = UI_FS_BADGE_TYPE_BADGE_IMAGE;
        }
    }
    
    // Parse fields for "badge" type
    if (badge_item->type == UI_FS_BADGE_TYPE_BADGE) {
        cJSON *firstname = cJSON_GetObjectItem(item_json, "firstname");
        if (cJSON_IsString(firstname)) {
            safe_strcpy(badge_item->firstname, cJSON_GetStringValue(firstname), UI_FS_BADGE_STRING_MAX_LEN);
        }
        
        cJSON *lastname = cJSON_GetObjectItem(item_json, "lastname");
        if (cJSON_IsString(lastname)) {
            safe_strcpy(badge_item->lastname, cJSON_GetStringValue(lastname), UI_FS_BADGE_STRING_MAX_LEN);
        }
        
        cJSON *title = cJSON_GetObjectItem(item_json, "title");
        if (cJSON_IsString(title)) {
            safe_strcpy(badge_item->title, cJSON_GetStringValue(title), UI_FS_BADGE_STRING_MAX_LEN);
        }
        
        cJSON *email = cJSON_GetObjectItem(item_json, "email");
        if (cJSON_IsString(email)) {
            safe_strcpy(badge_item->email, cJSON_GetStringValue(email), UI_FS_BADGE_STRING_MAX_LEN);
        }
        
        cJSON *phone = cJSON_GetObjectItem(item_json, "phone");
        if (cJSON_IsString(phone)) {
            safe_strcpy(badge_item->phone, cJSON_GetStringValue(phone), UI_FS_BADGE_STRING_MAX_LEN);
        }
        
        cJSON *site = cJSON_GetObjectItem(item_json, "site");
        if (cJSON_IsString(site)) {
            safe_strcpy(badge_item->site, cJSON_GetStringValue(site), UI_FS_BADGE_STRING_MAX_LEN);
        }
        
        cJSON *company = cJSON_GetObjectItem(item_json, "company");
        if (cJSON_IsString(company)) {
            safe_strcpy(badge_item->company, cJSON_GetStringValue(company), UI_FS_BADGE_STRING_MAX_LEN);
        }
        
        cJSON *profile_picture = cJSON_GetObjectItem(item_json, "profile_picture");
        if (cJSON_IsString(profile_picture)) {
            safe_strcpy(badge_item->profile_picture, cJSON_GetStringValue(profile_picture), UI_FS_BADGE_STRING_MAX_LEN);
        }
        
        cJSON *slogan = cJSON_GetObjectItem(item_json, "slogan");
        if (cJSON_IsString(slogan)) {
            safe_strcpy(badge_item->slogan, cJSON_GetStringValue(slogan), UI_FS_BADGE_STRING_MAX_LEN);
        }
        
        cJSON *image_QR = cJSON_GetObjectItem(item_json, "image_QR");
        if (cJSON_IsString(image_QR)) {
            safe_strcpy(badge_item->image_QR, cJSON_GetStringValue(image_QR), UI_FS_BADGE_STRING_MAX_LEN);
        }

        // Parse NFC data type and NFC data
        cJSON *nfc_data_type = cJSON_GetObjectItem(item_json, "nfc_data_type");
        if (cJSON_IsString(nfc_data_type)) {
            safe_strcpy(badge_item->nfc_data_type, cJSON_GetStringValue(nfc_data_type), UI_FS_BADGE_STRING_MAX_LEN);
        }

        cJSON *nfc_data = cJSON_GetObjectItem(item_json, "nfc_data");
        if (cJSON_IsString(nfc_data)) {
            safe_strcpy(badge_item->nfc_data, cJSON_GetStringValue(nfc_data), UI_FS_BADGE_STRING_MAX_LEN);
        }
    }
    // Parse fields for "badge_image" type
    else if (badge_item->type == UI_FS_BADGE_TYPE_BADGE_IMAGE) {
        cJSON *image_path = cJSON_GetObjectItem(item_json, "image_path");
        if (cJSON_IsString(image_path)) {
            safe_strcpy(badge_item->image_path, cJSON_GetStringValue(image_path), UI_FS_BADGE_STRING_MAX_LEN);
        }
        
        cJSON *nfc_data_type = cJSON_GetObjectItem(item_json, "nfc_data_type");
        if (cJSON_IsString(nfc_data_type)) {
            safe_strcpy(badge_item->nfc_data_type, cJSON_GetStringValue(nfc_data_type), UI_FS_BADGE_STRING_MAX_LEN);
        }
        
        cJSON *nfc_data = cJSON_GetObjectItem(item_json, "nfc_data");
        if (cJSON_IsString(nfc_data)) {
            safe_strcpy(badge_item->nfc_data, cJSON_GetStringValue(nfc_data), UI_FS_BADGE_STRING_MAX_LEN);
        }
    }
}

static int parse_badge_json(const char *json_string)
{
    if (json_string == NULL || g_badge_data == NULL) {
        return -1;
    }
    
    cJSON *json = cJSON_Parse(json_string);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            LV_LOG_ERROR("JSON parse error: %s", error_ptr);
        }
        return -1;
    }
    
    // Reset badge data
    memset(g_badge_data, 0, sizeof(ui_fs_badge_data_t));
    
    // Parse default_badge_id
    cJSON *default_badge_id = cJSON_GetObjectItem(json, "default_badge_id");
    if (cJSON_IsNumber(default_badge_id)) {
        g_badge_data->default_badge_id = (uint8_t)cJSON_GetNumberValue(default_badge_id);
    }
    
    // Parse badge_list
    cJSON *badge_list = cJSON_GetObjectItem(json, "badge_list");
    if (cJSON_IsArray(badge_list)) {
        int array_size = cJSON_GetArraySize(badge_list);
        g_badge_data->badge_count = 0;
        
        for (int i = 0; i < array_size && g_badge_data->badge_count < UI_FS_BADGE_MAX_BADGE_COUNT; i++) {
            cJSON *item = cJSON_GetArrayItem(badge_list, i);
            if (item != NULL) {
                parse_badge_item(item, &g_badge_data->badge_list[g_badge_data->badge_count]);
                g_badge_data->badge_count++;
            }
        }
    }
    
    cJSON_Delete(json);
    return 0;
}

int ui_fs_badge_init(const char *json_path)
{
    if (json_path == NULL) {
        return -1;
    }
    
    // Free existing data if already initialized
    if (g_badge_data != NULL) {
        LOCAL_FREE(g_badge_data);
        g_badge_data = NULL;
    }
    
    // Allocate memory for badge data
    g_badge_data = (ui_fs_badge_data_t *)LOCAL_MALLOC(sizeof(ui_fs_badge_data_t));
    if (g_badge_data == NULL) {
        LV_LOG_ERROR("Failed to allocate memory for badge data");
        return -1;
    }
    
    memset(g_badge_data, 0, sizeof(ui_fs_badge_data_t));
    
    // Check if file exists
    BOOL_T is_exist = FALSE;
    tkl_fs_is_exist(json_path, &is_exist);
    if (!is_exist) {
        LV_LOG_ERROR("Badge JSON file does not exist: %s", json_path);
        LOCAL_FREE(g_badge_data);
        g_badge_data = NULL;
        return -1;
    }
    
    // Read file
    TUYA_FILE fp = tkl_fopen(json_path, "r");
    if (fp == NULL) {
        LV_LOG_ERROR("Failed to open badge JSON file: %s", json_path);
        LOCAL_FREE(g_badge_data);
        g_badge_data = NULL;
        return -1;
    }
    
    // Get file size
    tkl_fseek(fp, 0, SEEK_END);
    int64_t file_size = tkl_ftell(fp);
    tkl_fseek(fp, 0, SEEK_SET);
    
    if (file_size <= 0) {
        LV_LOG_ERROR("Badge JSON file is empty: %s", json_path);
        tkl_fclose(fp);
        LOCAL_FREE(g_badge_data);
        g_badge_data = NULL;
        return -1;
    }
    
    // Allocate buffer for file content
    char *json_buffer = (char *)LOCAL_MALLOC(file_size + 1);
    if (json_buffer == NULL) {
        LV_LOG_ERROR("Failed to allocate memory for JSON buffer");
        tkl_fclose(fp);
        LOCAL_FREE(g_badge_data);
        g_badge_data = NULL;
        return -1;
    }
    
    // Read file content
    int bytes_read = tkl_fread(json_buffer, file_size, fp);
    tkl_fclose(fp);
    
    if (bytes_read != file_size) {
        LV_LOG_ERROR("Failed to read complete badge JSON file");
        LOCAL_FREE(json_buffer);
        LOCAL_FREE(g_badge_data);
        g_badge_data = NULL;
        return -1;
    }
    
    json_buffer[file_size] = '\0';
    
    // Parse JSON
    int ret = parse_badge_json(json_buffer);
    
    LOCAL_FREE(json_buffer);
    
    if (ret != 0) {
        LV_LOG_ERROR("Failed to parse badge JSON");
        LOCAL_FREE(g_badge_data);
        g_badge_data = NULL;
        return -1;
    }
    
    LV_LOG_USER("Badge JSON initialized successfully, badge count: %u", g_badge_data->badge_count);
    return 0;
}

int ui_fs_badge_read(uint8_t id, ui_fs_badge_item_t *badge_item)
{
    if (badge_item == NULL || g_badge_data == NULL) {
        return -1;
    }
    
    // Search for badge with matching ID
    for (uint32_t i = 0; i < g_badge_data->badge_count; i++) {
        if (g_badge_data->badge_list[i].id == id) {
            memcpy(badge_item, &g_badge_data->badge_list[i], sizeof(ui_fs_badge_item_t));

#if 1
            if (badge_item->type == UI_FS_BADGE_TYPE_BADGE) {
                LV_LOG_USER("Badge with ID %u read successfully", id);
                LV_LOG_USER("Badge item: firstname = %s", badge_item->firstname);
                LV_LOG_USER("Badge item: lastname = %s", badge_item->lastname);
                LV_LOG_USER("Badge item: title = %s", badge_item->title);
                LV_LOG_USER("Badge item: email = %s", badge_item->email);
                LV_LOG_USER("Badge item: phone = %s", badge_item->phone);
                LV_LOG_USER("Badge item: site = %s", badge_item->site);
                LV_LOG_USER("Badge item: company = %s", badge_item->company);
                LV_LOG_USER("Badge item: profile_picture = %s", badge_item->profile_picture);
                LV_LOG_USER("Badge item: slogan = %s", badge_item->slogan);
                LV_LOG_USER("Badge item: image_QR = %s", badge_item->image_QR);
            } else if (badge_item->type == UI_FS_BADGE_TYPE_BADGE_IMAGE) {
                LV_LOG_USER("Badge with ID %u read successfully", id);
                LV_LOG_USER("Badge item: image_path = %s", badge_item->image_path);
                LV_LOG_USER("Badge item: nfc_data_type = %s", badge_item->nfc_data_type);
                LV_LOG_USER("Badge item: nfc_data = %s", badge_item->nfc_data);
            }
#endif

            return 0;
        }
    }
    
    LV_LOG_WARN("Badge with ID %u not found", id);
    return -1;
}

uint32_t ui_fs_badge_list_number_get(void)
{
    if (g_badge_data == NULL) {
        return 0;
    }
    return g_badge_data->badge_count;
}

void ui_fs_badge_deinit(void)
{
    if (g_badge_data != NULL) {
        LOCAL_FREE(g_badge_data);
        g_badge_data = NULL;
    }
}