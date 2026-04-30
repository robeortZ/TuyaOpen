/**
 * @file app_sdcard_demo.c
 * @brief SD card read/write demo implementation for 128MB test
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_sdcard_demo.h"
#include "app_hardware.h"
#include "tkl_fs.h"
#include "tkl_memory.h"
#include "tal_log.h"
#include "tal_api.h"
#include <string.h>
#include <stdio.h>

/***********************************************************
***********************variable define**********************
***********************************************************/
static app_sdcard_demo_status_cb_t   sg_status_cb   = NULL;
static app_sdcard_demo_progress_cb_t sg_progress_cb = NULL;
static app_sdcard_demo_result_cb_t   sg_result_cb   = NULL;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Set callback function for SD card status updates
 */
__attribute__((unused)) void app_sdcard_demo_set_status_callback(app_sdcard_demo_status_cb_t cb)
{
    sg_status_cb = cb;
}

/**
 * @brief Set callback function for SD card test progress updates
 */
__attribute__((unused)) void app_sdcard_demo_set_progress_callback(app_sdcard_demo_progress_cb_t cb)
{
    sg_progress_cb = cb;
}

/**
 * @brief Set callback function for SD card test results
 */
__attribute__((unused)) void app_sdcard_demo_set_result_callback(app_sdcard_demo_result_cb_t cb)
{
    sg_result_cb = cb;
}

/**
 * @brief Write "hello world" text file to SD card
 *
 * Note: Kept for direct use even though primarily called via app_sdcard_demo_run_all()
 */
__attribute__((unused)) OPERATE_RET app_sdcard_demo_write_file(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_FILE   file_hdl;
    const char *text     = "hello world\n";
    size_t      text_len = strlen(text);
    uint32_t    written  = 0;

    PR_NOTICE("=== SD Card Demo: Writing File ===");

    // Update UI status
    if (sg_status_cb != NULL) {
        sg_status_cb("Writing hello_world.txt...");
    }

    // Check if SD card is mounted
    if (!app_sdcard_is_mounted()) {
        PR_ERR("SD card is not mounted");
        if (sg_status_cb != NULL) {
            sg_status_cb("Error: SD card not mounted");
        }
        return OPRT_COM_ERROR;
    }

    // Open file for writing
    file_hdl = tkl_fopen(SDCARD_DEMO_FILE, "w");
    if (NULL == file_hdl) {
        PR_ERR("Failed to open file %s for writing", SDCARD_DEMO_FILE);
        return OPRT_COM_ERROR;
    }

    // Write text to file
    written = tkl_fwrite((void *)text, text_len, file_hdl);
    if (written != text_len) {
        PR_ERR("Failed to write to file: wrote %d/%zu bytes", written, text_len);
        tkl_fclose(file_hdl);
        return OPRT_COM_ERROR;
    }

    // Close file
    rt = tkl_fclose(file_hdl);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to close file: %d", rt);
        return rt;
    }

    PR_NOTICE("Successfully wrote %zu bytes to %s", written, SDCARD_DEMO_FILE);

    // Update UI status
    if (sg_status_cb != NULL) {
        char status[128];
        snprintf(status, sizeof(status), "File written: %zu bytes", written);
        sg_status_cb(status);
    }

    return OPRT_OK;
}

/**
 * @brief List directory contents
 *
 * Note: Kept for direct use even though primarily called via app_sdcard_demo_run_all()
 */
__attribute__((unused)) OPERATE_RET app_sdcard_demo_list_dir(const char *path)
{
    TUYA_DIR      dir;
    TUYA_FILEINFO info;
    const char   *name;
    BOOL_T        is_dir = FALSE;

    if (NULL == path) {
        return OPRT_INVALID_PARM;
    }

    // Check if SD card is mounted
    if (!app_sdcard_is_mounted()) {
        PR_ERR("SD card is not mounted");
        return OPRT_COM_ERROR;
    }

    PR_NOTICE("=== SD Card Demo: Listing Directory ===");
    PR_NOTICE("Directory: %s", path);

    // Open directory
    if (tkl_dir_open(path, &dir) != 0) {
        PR_ERR("Failed to open directory %s", path);
        return OPRT_COM_ERROR;
    }

    // Read directory entries
    while (tkl_dir_read(dir, &info) == 0) {
        if (tkl_dir_name(info, &name) != 0) {
            continue;
        }

        // Skip . and .. entries
        if (name[0] == '.') {
            continue;
        }

        // Check if it's a directory
        if (tkl_dir_is_directory(info, &is_dir) == 0) {
            if (is_dir) {
                PR_NOTICE("  [DIR]  %s", name);
            } else {
                PR_NOTICE("  [FILE] %s", name);
            }
        } else {
            PR_NOTICE("  [ENTRY] %s", name);
        }
    }

    // Close directory
    tkl_dir_close(dir);

    PR_NOTICE("Directory listing complete");
    return OPRT_OK;
}

/**
 * @brief Recursively list all directories and files
 *
 * Note: Kept for direct use even though primarily called via app_sdcard_demo_run_all()
 */
__attribute__((unused)) OPERATE_RET app_sdcard_demo_list_dir_recursive(const char *path, int depth)
{
    TUYA_DIR      dir;
    TUYA_FILEINFO info;
    const char   *name;
    char          full_path[256];
    char          indent[64] = {0};

    if (NULL == path) {
        return OPRT_INVALID_PARM;
    }

    // Check if SD card is mounted
    if (!app_sdcard_is_mounted()) {
        PR_ERR("SD card is not mounted");
        return OPRT_COM_ERROR;
    }

    // Create indentation based on depth
    for (int i = 0; i < depth && i < (int)(sizeof(indent) - 1); i++) {
        indent[i]     = ' ';
        indent[i + 1] = ' ';
    }
    indent[depth * 2] = '\0';

    // Print current directory
    if (depth == 0) {
        PR_NOTICE("=== SD Card Demo: Recursive Directory Listing ===");
    }
    PR_NOTICE("%s[DIR] %s", indent, path);

    // Open directory
    if (tkl_dir_open(path, &dir) != 0) {
        PR_ERR("%sFailed to open directory %s", indent, path);
        return OPRT_COM_ERROR;
    }

    // Read directory entries
    while (tkl_dir_read(dir, &info) == 0) {
        if (tkl_dir_name(info, &name) != 0) {
            continue;
        }

        // Skip . and .. entries
        if (name[0] == '.') {
            continue;
        }

        // Build full path
        snprintf(full_path, sizeof(full_path), "%s/%s", path, name);

        // Check if it's a directory
        BOOL_T is_dir = FALSE;
        if (tkl_dir_is_directory(info, &is_dir) == 0) {
            if (is_dir) {
                // Recursively list subdirectory
                app_sdcard_demo_list_dir_recursive(full_path, depth + 1);
            } else {
                PR_NOTICE("%s  [FILE] %s", indent, name);
            }
        } else {
            PR_NOTICE("%s  [ENTRY] %s", indent, name);
        }
    }

    // Close directory
    tkl_dir_close(dir);

    return OPRT_OK;
}

/**
 * @brief SD card speed test: Write and read 128MB file
 *
 * This function performs a comprehensive speed test:
 * 1. Writes 128MB of data to a file
 * 2. Reads 128MB of data from the file
 * 3. Calculates and reports read/write speeds in MB/s
 *
 * Note: Kept for direct use even though primarily called via app_sdcard_demo_run_all()
 */
__attribute__((unused)) OPERATE_RET app_sdcard_demo_speed_test(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_FILE   file_hdl;
    uint32_t    start_time, end_time, elapsed_ms;
    uint32_t    bytes_written = 0, bytes_read = 0;
    uint32_t    total_written = 0, total_read = 0;
    float       write_speed_mbps = 0.0f, read_speed_mbps = 0.0f;
    uint8_t    *write_buffer = NULL;
    uint8_t    *read_buffer  = NULL;
    uint32_t    buffer_size  = SDCARD_DEMO_BUFFER_SIZE;
    uint32_t    remaining    = SDCARD_DEMO_SIZE_BYTES;

    // Check if SD card is mounted
    if (!app_sdcard_is_mounted()) {
        PR_ERR("SD card is not mounted");
        return OPRT_COM_ERROR;
    }

    PR_NOTICE("=== SD Card Speed Test ===");
    PR_NOTICE("Test file: %s", SDCARD_SPEED_TEST_FILE);
    PR_NOTICE("Test size: %d MB (%d bytes)", SDCARD_DEMO_SIZE_MB, SDCARD_DEMO_SIZE_BYTES);
    PR_NOTICE("Buffer size: %d bytes", buffer_size);

    // Update UI status
    if (sg_status_cb != NULL) {
        char status[128];
        snprintf(status, sizeof(status), "Speed Test: %d MB", SDCARD_DEMO_SIZE_MB);
        sg_status_cb(status);
    }

    // Allocate write buffer
    write_buffer = (uint8_t *)tkl_system_malloc(buffer_size);
    if (NULL == write_buffer) {
        PR_ERR("Failed to allocate write buffer (%d bytes)", buffer_size);
        return OPRT_MALLOC_FAILED;
    }

    // Allocate read buffer
    read_buffer = (uint8_t *)tkl_system_malloc(buffer_size);
    if (NULL == read_buffer) {
        PR_ERR("Failed to allocate read buffer (%d bytes)", buffer_size);
        tkl_system_free(write_buffer);
        return OPRT_MALLOC_FAILED;
    }

    // Initialize write buffer with pattern (for verification)
    for (uint32_t i = 0; i < buffer_size; i++) {
        write_buffer[i] = (uint8_t)(i & 0xFF);
    }

    // ========== WRITE TEST ==========
    PR_NOTICE("\n--- Write Test ---");
    PR_NOTICE("Writing %d MB to %s...", SDCARD_DEMO_SIZE_MB, SDCARD_SPEED_TEST_FILE);

    // Open file for writing
    file_hdl = tkl_fopen(SDCARD_SPEED_TEST_FILE, "w");
    if (NULL == file_hdl) {
        PR_ERR("Failed to open file %s for writing", SDCARD_SPEED_TEST_FILE);
        tkl_system_free(write_buffer);
        tkl_system_free(read_buffer);
        return OPRT_COM_ERROR;
    }

    // Start write timer
    start_time    = tal_system_get_millisecond();
    total_written = 0;
    remaining     = SDCARD_DEMO_SIZE_BYTES;

    // Write data in chunks
    while (remaining > 0) {
        uint32_t chunk_size = (remaining > buffer_size) ? buffer_size : remaining;

        bytes_written = tkl_fwrite(write_buffer, chunk_size, file_hdl);
        if (bytes_written != chunk_size) {
            PR_ERR("Write failed: wrote %d/%d bytes at offset %d", bytes_written, chunk_size, total_written);
            tkl_fclose(file_hdl);
            tkl_system_free(write_buffer);
            tkl_system_free(read_buffer);
            return OPRT_COM_ERROR;
        }

        total_written += bytes_written;
        remaining -= bytes_written;

        // Progress indicator every 10%
        if ((total_written % (SDCARD_DEMO_SIZE_BYTES / 10)) < chunk_size) {
            uint32_t percent = (total_written * 100) / SDCARD_DEMO_SIZE_BYTES;
            PR_NOTICE("Write progress: %d%% (%d MB / %d MB)", percent, total_written / (1024 * 1024),
                      SDCARD_DEMO_SIZE_MB);

            // Update UI progress
            if (sg_progress_cb != NULL) {
                sg_progress_cb("Writing", percent);
            }
        }
    }

    // Close file and stop timer
    rt = tkl_fclose(file_hdl);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to close file after write: %d", rt);
        tkl_system_free(write_buffer);
        tkl_system_free(read_buffer);
        return rt;
    }

    end_time   = tal_system_get_millisecond();
    elapsed_ms = end_time - start_time;

    // Calculate write speed
    if (elapsed_ms > 0) {
        write_speed_mbps = ((float)total_written / (1024.0f * 1024.0f)) / ((float)elapsed_ms / 1000.0f);
    }

    PR_NOTICE("Write completed: %d bytes in %d ms", total_written, elapsed_ms);
    PR_NOTICE("Write speed: %.2f MB/s", write_speed_mbps);

    // ========== READ TEST ==========
    PR_NOTICE("\n--- Read Test ---");
    PR_NOTICE("Reading %d MB from %s...", SDCARD_DEMO_SIZE_MB, SDCARD_SPEED_TEST_FILE);

    // Open file for reading
    file_hdl = tkl_fopen(SDCARD_SPEED_TEST_FILE, "r");
    if (NULL == file_hdl) {
        PR_ERR("Failed to open file %s for reading", SDCARD_SPEED_TEST_FILE);
        tkl_system_free(write_buffer);
        tkl_system_free(read_buffer);
        return OPRT_COM_ERROR;
    }

    // Start read timer
    start_time = tal_system_get_millisecond();
    total_read = 0;
    remaining  = SDCARD_DEMO_SIZE_BYTES;

    // Read data in chunks
    while (remaining > 0) {
        uint32_t chunk_size = (remaining > buffer_size) ? buffer_size : remaining;

        bytes_read = tkl_fread(read_buffer, chunk_size, file_hdl);
        if (bytes_read != chunk_size) {
            PR_ERR("Read failed: read %d/%d bytes at offset %d", bytes_read, chunk_size, total_read);
            tkl_fclose(file_hdl);
            tkl_system_free(write_buffer);
            tkl_system_free(read_buffer);
            return OPRT_COM_ERROR;
        }

        total_read += bytes_read;
        remaining -= bytes_read;

        // Progress indicator every 10%
        if ((total_read % (SDCARD_DEMO_SIZE_BYTES / 10)) < chunk_size) {
            uint32_t percent = (total_read * 100) / SDCARD_DEMO_SIZE_BYTES;
            PR_NOTICE("Read progress: %d%% (%d MB / %d MB)", percent, total_read / (1024 * 1024), SDCARD_DEMO_SIZE_MB);

            // Update UI progress
            if (sg_progress_cb != NULL) {
                sg_progress_cb("Reading", percent);
            }
        }
    }

    // Close file and stop timer
    rt = tkl_fclose(file_hdl);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to close file after read: %d", rt);
        tkl_system_free(write_buffer);
        tkl_system_free(read_buffer);
        return rt;
    }

    end_time   = tal_system_get_millisecond();
    elapsed_ms = end_time - start_time;

    // Calculate read speed
    if (elapsed_ms > 0) {
        read_speed_mbps = ((float)total_read / (1024.0f * 1024.0f)) / ((float)elapsed_ms / 1000.0f);
    }

    PR_NOTICE("Read completed: %d bytes in %d ms", total_read, elapsed_ms);
    PR_NOTICE("Read speed: %.2f MB/s", read_speed_mbps);

    // ========== SUMMARY ==========
    PR_NOTICE("\n--- Speed Test Summary ---");
    PR_NOTICE("File: %s", SDCARD_SPEED_TEST_FILE);
    PR_NOTICE("Size: %d MB (%d bytes)", SDCARD_DEMO_SIZE_MB, SDCARD_DEMO_SIZE_BYTES);
    PR_NOTICE("Write: %.2f MB/s (%d ms)", write_speed_mbps,
              (uint32_t)((total_written / (1024.0f * 1024.0f)) / write_speed_mbps * 1000.0f));
    PR_NOTICE("Read:  %.2f MB/s (%d ms)", read_speed_mbps,
              (uint32_t)((total_read / (1024.0f * 1024.0f)) / read_speed_mbps * 1000.0f));
    PR_NOTICE("===========================");

    // Update UI with results
    if (sg_result_cb != NULL) {
        sg_result_cb("128MB Speed Test", write_speed_mbps, read_speed_mbps);
    }
    if (sg_status_cb != NULL) {
        char status[128];
        snprintf(status, sizeof(status), "Test Complete: W:%.2f R:%.2f MB/s", write_speed_mbps, read_speed_mbps);
        sg_status_cb(status);
    }

    // Cleanup
    tkl_system_free(write_buffer);
    tkl_system_free(read_buffer);

    return OPRT_OK;
}

/**
 * @brief Run all SD card demo functions
 */
__attribute__((unused)) OPERATE_RET app_sdcard_demo_run_all(void)
{
    OPERATE_RET rt         = OPRT_OK;
    const char *mount_path = NULL;

    PR_NOTICE("\n=== SD Card Demo: Starting All Tests ===");

    // Check if SD card is mounted
    if (!app_sdcard_is_mounted()) {
        PR_ERR("SD card is not mounted. Please initialize SD card first.");
        if (sg_status_cb != NULL) {
            sg_status_cb("Error: SD card not mounted");
        }
        return OPRT_COM_ERROR;
    }

    mount_path = app_sdcard_get_mount_path();
    PR_NOTICE("SD card mounted at: %s", mount_path);

    // // Update UI status
    // if (sg_status_cb != NULL) {
    //     char status[128];
    //     snprintf(status, sizeof(status), "SD Card: %s", mount_path);
    //     sg_status_cb(status);
    // }

    // Wait a bit for SD card to be ready
    tal_system_sleep(200);

    // // Write hello world file
    // PR_NOTICE("\n--- Step 1: Write Hello World File ---");
    // if (sg_progress_cb != NULL) {
    //     sg_progress_cb("Step 1: Write File", 0);
    // }
    // rt = app_sdcard_demo_write_file();
    // if (OPRT_OK != rt) {
    //     PR_ERR("Failed to write hello world file: %d", rt);
    //     return rt;
    // }
    // tal_system_sleep(200);

    // // List root directory
    // PR_NOTICE("\n--- Step 2: List Root Directory ---");
    // if (sg_progress_cb != NULL) {
    //     sg_progress_cb("Step 2: List Dir", 25);
    // }
    // rt = app_sdcard_demo_list_dir(mount_path);
    // if (OPRT_OK != rt) {
    //     PR_ERR("Failed to list directory: %d", rt);
    //     return rt;
    // }
    // tal_system_sleep(200);

    // Recursively list all directories
    PR_NOTICE("\n--- Step 3: Recursive Directory Listing ---");
    if (sg_progress_cb != NULL) {
        sg_progress_cb("Step 3: Recursive", 50);
    }
    rt = app_sdcard_demo_list_dir_recursive(mount_path, 0);
    if (OPRT_OK != rt) {
        PR_ERR("Failed to list directories recursively: %d", rt);
        return rt;
    }
    tal_system_sleep(500);

    // // Speed test: Write and read 128MB
    // PR_NOTICE("\n--- Step 4: 128MB Speed Test ---");
    // PR_NOTICE("Starting SD card speed test (this may take several minutes)...");
    // if (sg_progress_cb != NULL) {
    //     sg_progress_cb("Step 4: Speed Test", 75);
    // }
    // rt = app_sdcard_demo_speed_test();
    // if (OPRT_OK != rt) {
    //     PR_ERR("Speed test failed: %d", rt);
    //     return rt;
    // }

    // PR_NOTICE("\n=== SD Card Demo: All Tests Completed ===");

    // // Update UI status
    // if (sg_status_cb != NULL) {
    //     sg_status_cb("All tests completed!");
    // }
    // if (sg_progress_cb != NULL) {
    //     sg_progress_cb("Complete", 100);
    // }

    return OPRT_OK;
}
