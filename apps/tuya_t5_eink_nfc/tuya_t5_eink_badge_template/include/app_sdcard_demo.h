/**
 * @file app_sdcard_demo.h
 * @brief SD card read/write demo functions for 128MB test
 *
 * This module provides demo functions to test SD card read/write operations:
 * - Write a simple text file
 * - List directory contents
 * - Recursive directory listing
 * - Speed test: Write and read 128MB file
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef APP_SDCARD_DEMO_H
#define APP_SDCARD_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tuya_cloud_types.h"

/***********************************************************
************************macro define************************
***********************************************************/
// SD card demo file paths
#define SDCARD_DEMO_FILE       "/sdcard/hello_world.txt"
#define SDCARD_SPEED_TEST_FILE "/sdcard/speed_test_128mb.bin"

// Speed test configuration (128MB)
#define SDCARD_DEMO_SIZE_MB     128
#define SDCARD_DEMO_SIZE_BYTES  (SDCARD_DEMO_SIZE_MB * 1024 * 1024)
#define SDCARD_DEMO_BUFFER_SIZE (64 * 1024) // 64KB buffer for read/write operations

/***********************************************************
***********************typedef define***********************
***********************************************************/
// UI update callback function types
typedef void (*app_sdcard_demo_status_cb_t)(const char *status_text);
typedef void (*app_sdcard_demo_progress_cb_t)(const char *step_name, uint32_t percent);
typedef void (*app_sdcard_demo_result_cb_t)(const char *test_name, float write_speed, float read_speed);

/***********************************************************
********************function declaration********************
***********************************************************/

/**
 * @brief Set callback function for SD card status updates
 *
 * @param cb Callback function to receive status text updates
 */
void app_sdcard_demo_set_status_callback(app_sdcard_demo_status_cb_t cb);

/**
 * @brief Set callback function for SD card test progress updates
 *
 * @param cb Callback function to receive progress updates (step name and percent)
 */
void app_sdcard_demo_set_progress_callback(app_sdcard_demo_progress_cb_t cb);

/**
 * @brief Set callback function for SD card test results
 *
 * @param cb Callback function to receive test results (test name, write speed, read speed)
 */
void app_sdcard_demo_set_result_callback(app_sdcard_demo_result_cb_t cb);

/**
 * @brief Write "hello world" text file to SD card
 *
 * Note: This function is called by app_sdcard_demo_run_all(), but can also
 * be called directly for individual testing.
 *
 * @return OPERATE_RET_OK on success, error code otherwise
 */
OPERATE_RET app_sdcard_demo_write_file(void) __attribute__((used));

/**
 * @brief List directory contents
 *
 * Note: This function is called by app_sdcard_demo_run_all(), but can also
 * be called directly for individual testing.
 *
 * @param path Directory path to list
 * @return OPERATE_RET_OK on success, error code otherwise
 */
OPERATE_RET app_sdcard_demo_list_dir(const char *path) __attribute__((used));

/**
 * @brief Recursively list all directories and files
 *
 * Note: This function is called by app_sdcard_demo_run_all(), but can also
 * be called directly for individual testing.
 *
 * @param path Root directory path
 * @param depth Current recursion depth (start with 0)
 * @return OPERATE_RET_OK on success, error code otherwise
 */
OPERATE_RET app_sdcard_demo_list_dir_recursive(const char *path, int depth) __attribute__((used));

/**
 * @brief SD card speed test: Write and read 128MB file
 *
 * This function performs a comprehensive speed test:
 * 1. Writes 128MB of data to a file
 * 2. Reads 128MB of data from the file
 * 3. Calculates and reports read/write speeds in MB/s
 *
 * Note: This function is called by app_sdcard_demo_run_all(), but can also
 * be called directly for individual testing.
 *
 * @return OPERATE_RET_OK on success, error code otherwise
 */
OPERATE_RET app_sdcard_demo_speed_test(void) __attribute__((used));

/**
 * @brief Run all SD card demo functions
 *
 * This function runs all demo operations in sequence:
 * - Write hello world file
 * - List root directory
 * - Recursive directory listing
 * - 128MB speed test
 *
 * @return OPERATE_RET_OK on success, error code otherwise
 */
OPERATE_RET app_sdcard_demo_run_all(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SDCARD_DEMO_H */
