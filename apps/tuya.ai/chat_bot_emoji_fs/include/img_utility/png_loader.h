/**
* @file png_loader.h
* @brief PNG image loader header file
* @version 0.1
* @date 2024-01-01
*
* @copyright Copyright 2021-2030 Tuya Inc. All Rights Reserved.
*
*/

#ifndef __PNG_LOADER_H__
#define __PNG_LOADER_H__

#include <stdint.h>
#include <stdbool.h>
#include "tuya_cloud_types.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Frame buffer structure for image data
typedef struct {
    void *frame;        // Image data buffer
    uint32_t length;    // Data length
} frame_buffer_t;

// Image type enumeration
typedef enum {
    image_type_unknown = 0,
    image_type_jpg,
    image_type_jpeg,
    image_type_png,
    image_type_gif
} GUI_IMAGE_TYPE_E;

// Raw image loader function declaration
extern OPERATE_RET raw_img_load(GUI_IMAGE_TYPE_E image_type, uint8_t *img_data, uint32_t img_size, lv_img_dsc_t *img_dst, bool jpg_hw_dec);

/**
 * PNG图像文件加载函数
 * @brief Load PNG file from storage and store data in img_dst
 * @param[in] filename: File path and name
 * @param[in] img_dst: Image descriptor structure, if data is NULL, will allocate memory in PSRAM
 * @retval OPRT_OK: Success
 * @retval <0: Decode failed or file doesn't exist
 */
OPERATE_RET png_img_load(const char *filename, lv_img_dsc_t *img_dst);

/**
 * PNG图像文件卸载函数
 * @brief Unload PNG image and free allocated memory
 * @param[in] img_dst: Image descriptor structure
 */
void png_img_unload(lv_img_dsc_t *img_dst);

#ifdef __cplusplus
}
#endif

#endif /* __PNG_LOADER_H__ */ 