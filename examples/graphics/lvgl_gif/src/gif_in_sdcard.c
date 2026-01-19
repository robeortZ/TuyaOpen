/**
 * @file example_lvgl.c
 * @brief LVGL (Light and Versatile Graphics Library) example for SDK.
 *
 * This file provides an example implementation of using the LVGL library with the Tuya SDK.
 * It demonstrates the initialization and usage of LVGL for graphical user interface (GUI) development.
 * The example covers setting up the display port, initializing LVGL, and running a demo application.
 *
 * The LVGL example aims to help developers understand how to integrate LVGL into their Tuya IoT projects for
 * creating graphical user interfaces on embedded devices. It includes detailed examples of setting up LVGL,
 * handling display updates, and integrating these functionalities within a multitasking environment.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental LVGL
 * operations critical for GUI development on embedded systems.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_system.h"
#include "tkl_fs.h"
#include "lvgl.h"

// ！Load images into psram, must load
OPERATE_RET gui_img_load_psram(CHAR_T *filename, lv_img_dsc_t *img_dst)
{
    OPERATE_RET ret = OPRT_COM_ERROR;
    TUYA_FILE file_hdl = NULL;
    file_hdl = tkl_fopen(filename, "r");
    if (NULL == file_hdl) {
        PR_ERR("Failed to open file: %s\n", filename);
        return OPRT_COM_ERROR;
    }
    tkl_fseek(file_hdl, 0, SEEK_END);
    uint32_t file_size = tkl_ftell(file_hdl);
    
    // Check if file is empty
    if (file_size == 0) {
        PR_ERR("File is empty: %s\n", filename);
        tkl_fclose(file_hdl);
        return OPRT_COM_ERROR;
    }
    
    // Reset file pointer to the beginning for reading
    tkl_fseek(file_hdl, 0, SEEK_SET);

    // Allocate memory to store file content
    UINT8_T *buffer = tal_psram_malloc(file_size);
    if (buffer == NULL) {
        PR_ERR("Memory allocation failed\n");
        tkl_fclose(file_hdl);
        return OPRT_COM_ERROR;
    }

    uint32_t bytes_read;
    bytes_read = tkl_fread(buffer, file_size, file_hdl);
    if (bytes_read != file_size) {
        PR_ERR("Failed to read file: %s\n", filename);
        tal_psram_free(buffer);
        tkl_fclose(file_hdl);
        return OPRT_COM_ERROR;
    }else{
        img_dst->data = buffer;
        img_dst->data_size = file_size;
        ret = OPRT_OK;
        PR_DEBUG("File %s loaded successfully", filename);
    }
    tkl_fclose(file_hdl);
    
    return ret;
}

//! Unload images from psram, must unload
OPERATE_RET gui_img_unload_psram(lv_img_dsc_t *img_dsc)
{
    if (img_dsc->data != NULL) {
        tal_psram_free((void *)img_dsc->data);
        img_dsc->data = NULL;
    }
    return OPRT_OK;
}
