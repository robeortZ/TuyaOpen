/**
 * @file example_eink_nfc.c
 * @brief Demo application for TUYA_T5AI_EINK_NFC board
 *
 * This demo demonstrates:
 * - Hardware initialization
 * - User LED control (turned ON at startup)
 * - PWM backlight control at P33 (0-100% brightness, ±15% increments)
 * - Button event callbacks (UP/DOWN buttons control backlight brightness)
 * - Battery voltage and percentage reading
 * - Charge detection status
 * - Periodic status reporting
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_pwm.h"
#include "tkl_fs.h"
#include "tdl_button_manage.h"
#include "tdl_led_manage.h"
#include "board_com_api.h"
#include "board_charge_detect_api.h"
#include "tkl_memory.h"
#include "tdd_disp_uc8276.h"
#include "tdl_display_manage.h"
#include <string.h>
#include <stdio.h>

/***********************************************************
************************macro define************************
***********************************************************/
#define TASK_DEMO_PRIORITY    THREAD_PRIO_2
#define TASK_DEMO_SIZE        4096
#define STATUS_PRINT_INTERVAL 3000 // Print status every 3 seconds

// SD card paths
#define SDCARD_MOUNT_PATH      "/sdcard"
#define SDCARD_DEMO_FILE       "/sdcard/hello_world.txt"
#define SDCARD_SPEED_TEST_FILE "/sdcard/speed_test_512mb.bin"

// Speed test configuration
#define SPEED_TEST_SIZE_MB     1
#define SPEED_TEST_SIZE_BYTES  (SPEED_TEST_SIZE_MB * 1024 * 1024)
#define SPEED_TEST_BUFFER_SIZE (64 * 1024) // 64KB buffer for read/write operations

// Backlight PWM pin and configuration
#define BOARD_BACKLIGHT_PIN             TUYA_GPIO_NUM_33
#define BOARD_BACKLIGHT_PWM_CHANNEL     TUYA_PWM_NUM_9 // P33 uses PWM channel 9
#define BOARD_BACKLIGHT_PWM_FREQ        1000           // 1kHz frequency
#define BOARD_BACKLIGHT_BRIGHTNESS_MIN  0
#define BOARD_BACKLIGHT_BRIGHTNESS_MAX  100
#define BOARD_BACKLIGHT_BRIGHTNESS_STEP 15 // ±15% increment

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE     sg_demo_thrd_hdl;
static uint32_t          sg_button_event_count    = 0;
static TDL_BUTTON_HANDLE g_button_up_handle       = NULL;
static TDL_BUTTON_HANDLE g_button_down_handle     = NULL;
static TDL_BUTTON_HANDLE g_button_enter_handle    = NULL;
static TDL_BUTTON_HANDLE g_button_return_handle   = NULL;
static TDL_BUTTON_HANDLE g_button_left_handle     = NULL;
static TDL_BUTTON_HANDLE g_button_right_handle    = NULL;
static TDL_LED_HANDLE_T  sg_led_handle            = NULL;
static TUYA_PWM_NUM_E    sg_backlight_pwm_channel = TUYA_PWM_NUM_MAX;
static uint8_t           sg_backlight_brightness  = 0; // 0-100

/***********************************************************
********************function declaration********************
***********************************************************/
static void        button_demo_init_buttons(void);
static void        button_up_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);
static void        button_down_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);
static void        button_enter_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);
static void        button_return_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);
static void        button_left_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);
static void        button_right_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc);
static OPERATE_RET backlight_init(void);
static OPERATE_RET backlight_set_brightness(uint8_t brightness);
static OPERATE_RET backlight_increase_brightness(void);
static OPERATE_RET backlight_decrease_brightness(void);
static OPERATE_RET sdcard_demo_write_file(void);
static OPERATE_RET sdcard_demo_list_dir(const char *path);
static OPERATE_RET sdcard_demo_list_dir_recursive(const char *path, int depth);
static OPERATE_RET sdcard_demo_speed_test(void);
static OPERATE_RET eink_bw_test_pattern(void);
static OPERATE_RET eink_grayscale_test_pattern(void);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Button UP callback - increase backlight brightness
 */
static void button_up_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    if (event == TDL_BUTTON_PRESS_SINGLE_CLICK) {
        sg_button_event_count++;
        backlight_increase_brightness();
        PR_NOTICE("UP Button: Single click - brightness: %d%%", sg_backlight_brightness);
    }
}

/**
 * @brief Button DOWN callback - decrease backlight brightness
 */
static void button_down_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    if (event == TDL_BUTTON_PRESS_SINGLE_CLICK) {
        sg_button_event_count++;
        backlight_decrease_brightness();
        PR_NOTICE("DOWN Button: Single click - brightness: %d%%", sg_backlight_brightness);
    }
}

/**
 * @brief Button ENTER callback
 */
static void button_enter_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    if (event == TDL_BUTTON_PRESS_SINGLE_CLICK) {
        sg_button_event_count++;
        PR_NOTICE("ENTER Button: Single click detected (count: %lu)", sg_button_event_count);
    }
}

/**
 * @brief Button RETURN callback
 */
static void button_return_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    if (event == TDL_BUTTON_PRESS_SINGLE_CLICK) {
        sg_button_event_count++;
        PR_NOTICE("RETURN Button: Single click detected (count: %lu)", sg_button_event_count);
    }
}

/**
 * @brief Button LEFT callback
 */
static void button_left_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    if (event == TDL_BUTTON_PRESS_SINGLE_CLICK) {
        sg_button_event_count++;
        PR_NOTICE("LEFT Button: Single click detected (count: %lu)", sg_button_event_count);
    }
}

/**
 * @brief Button RIGHT callback
 */
static void button_right_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    if (event == TDL_BUTTON_PRESS_SINGLE_CLICK) {
        sg_button_event_count++;
        PR_NOTICE("RIGHT Button: Single click detected (count: %lu)", sg_button_event_count);
    }
}

/**
 * @brief Read and print battery status
 */
static void print_battery_status(void)
{
    OPERATE_RET rt;
    uint32_t    voltage_mv = 0;
    uint8_t     percentage = 0;

    return;

    PR_NOTICE("=== Battery Status ===");

    // Battery ADC should still work even if charge detect interrupt is disabled
    rt = board_battery_read(&voltage_mv, &percentage);
    if (OPRT_OK == rt) {
        PR_NOTICE("  Voltage:     %d mV (%.2f V)", voltage_mv, (float)voltage_mv / 1000.0f);
        PR_NOTICE("  Percentage:  %d%%", percentage);
    } else {
        PR_ERR("  Failed to read battery: %d", rt);
    }
}

/**
 * @brief Read and print charge status
 */
static void print_charge_status(void)
{
    PR_NOTICE("=== Charge Status ===");
    PR_NOTICE("  State:       (Charge detection disabled)");
}

/**
 * @brief Read and print button states
 */
static void print_button_states(void)
{
    PR_NOTICE("=== Button States ===");
    PR_NOTICE("  Total button events: %lu", sg_button_event_count);
    PR_NOTICE("  Backlight brightness: %d%%", sg_backlight_brightness);
}

/**
 * @brief Write "hello world" text file to SD card
 */
static OPERATE_RET sdcard_demo_write_file(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_FILE   file_hdl;
    const char *text     = "hello world\n";
    size_t      text_len = strlen(text);
    uint32_t    written  = 0;

    PR_NOTICE("=== SD Card Demo: Writing File ===");

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
    return OPRT_OK;
}

/**
 * @brief List directory contents
 */
static OPERATE_RET sdcard_demo_list_dir(const char *path)
{
    TUYA_DIR      dir;
    TUYA_FILEINFO info;
    const char   *name;
    BOOL_T        is_dir = FALSE;

    if (NULL == path) {
        return OPRT_INVALID_PARM;
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
 */
static OPERATE_RET sdcard_demo_list_dir_recursive(const char *path, int depth)
{
    TUYA_DIR      dir;
    TUYA_FILEINFO info;
    const char   *name;
    char          full_path[256];
    char          indent[64] = {0};

    if (NULL == path) {
        return OPRT_INVALID_PARM;
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
                sdcard_demo_list_dir_recursive(full_path, depth + 1);
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
 * @brief SD card speed test: Write and read 512MB file
 *
 * This function performs a comprehensive speed test:
 * 1. Writes 512MB of data to a file
 * 2. Reads 512MB of data from the file
 * 3. Calculates and reports read/write speeds in MB/s
 */
static OPERATE_RET sdcard_demo_speed_test(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_FILE   file_hdl;
    uint32_t    start_time, end_time, elapsed_ms;
    uint32_t    bytes_written = 0, bytes_read = 0;
    uint32_t    total_written = 0, total_read = 0;
    float       write_speed_mbps = 0.0f, read_speed_mbps = 0.0f;
    uint8_t    *write_buffer = NULL;
    uint8_t    *read_buffer  = NULL;
    uint32_t    buffer_size  = SPEED_TEST_BUFFER_SIZE;
    uint32_t    remaining    = SPEED_TEST_SIZE_BYTES;

    PR_NOTICE("=== SD Card Speed Test ===");
    PR_NOTICE("Test file: %s", SDCARD_SPEED_TEST_FILE);
    PR_NOTICE("Test size: %d MB (%d bytes)", SPEED_TEST_SIZE_MB, SPEED_TEST_SIZE_BYTES);
    PR_NOTICE("Buffer size: %d bytes", buffer_size);

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
    PR_NOTICE("Writing %d MB to %s...", SPEED_TEST_SIZE_MB, SDCARD_SPEED_TEST_FILE);

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
    remaining     = SPEED_TEST_SIZE_BYTES;

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
        if ((total_written % (SPEED_TEST_SIZE_BYTES / 10)) < chunk_size) {
            uint32_t percent = (total_written * 100) / SPEED_TEST_SIZE_BYTES;
            PR_NOTICE("Write progress: %d%% (%d MB / %d MB)", percent, total_written / (1024 * 1024),
                      SPEED_TEST_SIZE_MB);
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
    PR_NOTICE("Reading %d MB from %s...", SPEED_TEST_SIZE_MB, SDCARD_SPEED_TEST_FILE);

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
    remaining  = SPEED_TEST_SIZE_BYTES;

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
        if ((total_read % (SPEED_TEST_SIZE_BYTES / 10)) < chunk_size) {
            uint32_t percent = (total_read * 100) / SPEED_TEST_SIZE_BYTES;
            PR_NOTICE("Read progress: %d%% (%d MB / %d MB)", percent, total_read / (1024 * 1024), SPEED_TEST_SIZE_MB);
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
    PR_NOTICE("Size: %d MB (%d bytes)", SPEED_TEST_SIZE_MB, SPEED_TEST_SIZE_BYTES);
    PR_NOTICE("Write: %.2f MB/s (%d ms)", write_speed_mbps,
              (uint32_t)((total_written / (1024.0f * 1024.0f)) / write_speed_mbps * 1000.0f));
    PR_NOTICE("Read:  %.2f MB/s (%d ms)", read_speed_mbps,
              (uint32_t)((total_read / (1024.0f * 1024.0f)) / read_speed_mbps * 1000.0f));
    PR_NOTICE("===========================");

    // Cleanup
    tkl_system_free(write_buffer);
    tkl_system_free(read_buffer);

    return OPRT_OK;
}

/**
 * @brief Initialize buttons and register callbacks
 */
static void button_demo_init_buttons(void)
{
    OPERATE_RET      rt         = OPRT_OK;
    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time     = 2000, // 2 seconds for long press
                                   .long_keep_timer           = 500,
                                   .button_debounce_time      = 50,
                                   .button_repeat_valid_count = 0,
                                   .button_repeat_valid_time  = 500};

    // Initialize UP button
    PR_NOTICE("Initializing UP button with name: %s", BOARD_BUTTON_NAME_UP);
    rt = tdl_button_create(BOARD_BUTTON_NAME_UP, &button_cfg, &g_button_up_handle);
    if (OPRT_OK == rt) {
        PR_NOTICE("UP button created successfully, handle: %p", g_button_up_handle);
        tdl_button_event_register(g_button_up_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, button_up_cb);
        PR_NOTICE("UP button initialized and event registered successfully");
    } else {
        PR_ERR("Failed to create UP button '%s': %d", BOARD_BUTTON_NAME_UP, rt);
        PR_ERR("Make sure %s is registered in board_register_hardware()", BOARD_BUTTON_NAME_UP);
    }

    // Initialize DOWN button
    PR_NOTICE("Initializing DOWN button with name: %s", BOARD_BUTTON_NAME_DOWN);
    rt = tdl_button_create(BOARD_BUTTON_NAME_DOWN, &button_cfg, &g_button_down_handle);
    if (OPRT_OK == rt) {
        PR_NOTICE("DOWN button created successfully, handle: %p", g_button_down_handle);
        tdl_button_event_register(g_button_down_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, button_down_cb);
        PR_NOTICE("DOWN button initialized and event registered successfully");
    } else {
        PR_ERR("Failed to create DOWN button '%s': %d", BOARD_BUTTON_NAME_DOWN, rt);
        PR_ERR("Make sure %s is registered in board_register_hardware()", BOARD_BUTTON_NAME_DOWN);
    }

    // Initialize ENTER button
    PR_NOTICE("Initializing ENTER button with name: %s", BOARD_BUTTON_NAME_ENTER);
    rt = tdl_button_create(BOARD_BUTTON_NAME_ENTER, &button_cfg, &g_button_enter_handle);
    if (OPRT_OK == rt) {
        PR_NOTICE("ENTER button created successfully, handle: %p", g_button_enter_handle);
        tdl_button_event_register(g_button_enter_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, button_enter_cb);
        PR_NOTICE("ENTER button initialized and event registered successfully");
    } else {
        PR_ERR("Failed to create ENTER button '%s': %d", BOARD_BUTTON_NAME_ENTER, rt);
        PR_ERR("Make sure %s is registered in board_register_hardware()", BOARD_BUTTON_NAME_ENTER);
    }

    // Initialize RETURN button
    PR_NOTICE("Initializing RETURN button with name: %s", BOARD_BUTTON_NAME_RETURN);
    rt = tdl_button_create(BOARD_BUTTON_NAME_RETURN, &button_cfg, &g_button_return_handle);
    if (OPRT_OK == rt) {
        PR_NOTICE("RETURN button created successfully, handle: %p", g_button_return_handle);
        tdl_button_event_register(g_button_return_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, button_return_cb);
        PR_NOTICE("RETURN button initialized and event registered successfully");
    } else {
        PR_ERR("Failed to create RETURN button '%s': %d", BOARD_BUTTON_NAME_RETURN, rt);
        PR_ERR("Make sure %s is registered in board_register_hardware()", BOARD_BUTTON_NAME_RETURN);
    }

    // Initialize LEFT button
    PR_NOTICE("Initializing LEFT button with name: %s", BOARD_BUTTON_NAME_LEFT);
    rt = tdl_button_create(BOARD_BUTTON_NAME_LEFT, &button_cfg, &g_button_left_handle);
    if (OPRT_OK == rt) {
        PR_NOTICE("LEFT button created successfully, handle: %p", g_button_left_handle);
        tdl_button_event_register(g_button_left_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, button_left_cb);
        PR_NOTICE("LEFT button initialized and event registered successfully");
    } else {
        PR_ERR("Failed to create LEFT button '%s': %d", BOARD_BUTTON_NAME_LEFT, rt);
        PR_ERR("Make sure %s is registered in board_register_hardware()", BOARD_BUTTON_NAME_LEFT);
    }

    // Initialize RIGHT button
    PR_NOTICE("Initializing RIGHT button with name: %s", BOARD_BUTTON_NAME_RIGHT);
    rt = tdl_button_create(BOARD_BUTTON_NAME_RIGHT, &button_cfg, &g_button_right_handle);
    if (OPRT_OK == rt) {
        PR_NOTICE("RIGHT button created successfully, handle: %p", g_button_right_handle);
        tdl_button_event_register(g_button_right_handle, TDL_BUTTON_PRESS_SINGLE_CLICK, button_right_cb);
        PR_NOTICE("RIGHT button initialized and event registered successfully");
    } else {
        PR_ERR("Failed to create RIGHT button '%s': %d", BOARD_BUTTON_NAME_RIGHT, rt);
        PR_ERR("Make sure %s is registered in board_register_hardware()", BOARD_BUTTON_NAME_RIGHT);
    }
}

/**
 * @brief Demo task that periodically prints status
 */
static void demo_task(void *param)
{
    PR_NOTICE("Demo task started");

    while (1) {
        // Print status periodically
        PR_NOTICE("\n========== Status Report ==========");
        PR_NOTICE("Button Events: %lu", sg_button_event_count);
        print_charge_status();
        print_battery_status();
        print_button_states();
        PR_NOTICE("===================================\n");

        tal_system_sleep(STATUS_PRINT_INTERVAL);
    }
}

/**
 * @brief user_main
 *
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("========================================");
    PR_NOTICE("TUYA_T5AI_EINK_NFC Demo Application");
    PR_NOTICE("========================================");

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:   %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    // Initialize hardware
    rt = board_register_hardware();
    if (OPRT_OK != rt) {
        PR_ERR("board_register_hardware failed: %d", rt);
        return;
    }
    PR_NOTICE("Hardware initialized");

    // Wait a bit for hardware registration to complete
    tal_system_sleep(200);
    PR_NOTICE("After sleep 200ms");

    // Initialize and turn on user LED (optional, don't crash if it fails)
    sg_led_handle = tdl_led_find_dev("led_user");
    if (sg_led_handle != NULL) {
        rt = tdl_led_open(sg_led_handle);
        if (OPRT_OK == rt) {
            rt = tdl_led_set_status(sg_led_handle, TDL_LED_ON);
            if (OPRT_OK == rt) {
                PR_NOTICE("User LED turned ON");
            } else {
                PR_WARN("Failed to set LED status: %d (continuing)", rt);
            }
        } else {
            PR_WARN("Failed to open LED: %d (continuing)", rt);
        }
    } else {
        PR_WARN("LED device 'led_user' not found (continuing)");
    }

    // Initialize PWM backlight (optional, don't crash if it fails)
    // Delay a bit to ensure hardware is stable
    tal_system_sleep(100);
    rt = backlight_init();
    if (OPRT_OK == rt) {
        PR_NOTICE("Backlight initialized");
    } else {
        PR_WARN("Failed to initialize backlight: %d (continuing without backlight)", rt);
        // Set channel to invalid so button callbacks won't try to use it
        sg_backlight_pwm_channel = TUYA_PWM_NUM_MAX;
    }

    // Initialize buttons and register callbacks
    button_demo_init_buttons();

    // E-Ink Black/White Test Pattern (test basic display functionality)
    PR_NOTICE("\n=== E-Ink Black/White Test Pattern ===");

    // Open display first through TDD interface
    const char       *display_name = "display";
    TDL_DISP_HANDLE_T disp_handle  = tdl_disp_find_dev((char *)display_name);
    if (disp_handle != NULL) {
        PR_NOTICE("Opening display '%s'...", display_name);
        OPERATE_RET open_rt = tdl_disp_dev_open(disp_handle);
        if (OPRT_OK == open_rt) {
            PR_NOTICE("Display opened successfully");
        } else {
            PR_ERR("Failed to open display: %d", open_rt);
        }
    } else {
        PR_ERR("Display '%s' not found", display_name);
    }

    tal_system_sleep(1000); // Wait for display to be ready
    eink_bw_test_pattern();

    // Wait a bit before next test
    tal_system_sleep(2000);

    // E-Ink Grayscale Test Pattern (temporarily disable normal flush API)
    PR_NOTICE("\n=== E-Ink Grayscale Test Pattern ===");
    tal_system_sleep(1000); // Wait for display to be ready
    eink_grayscale_test_pattern();

    // SD Card Demo
    PR_NOTICE("\n=== SD Card Demo ===");
    rt = board_sdcard_init();
    if (OPRT_OK == rt) {
        PR_NOTICE("SD card initialized");

        // Mount SD card
        tal_system_sleep(500); // Wait for SD card to be ready
        rt = tkl_fs_mount(SDCARD_MOUNT_PATH, DEV_SDCARD);
        if (OPRT_OK == rt) {
            PR_NOTICE("SD card mounted successfully at %s", SDCARD_MOUNT_PATH);

            // Write hello world file
            tal_system_sleep(200);
            sdcard_demo_write_file();

            // List root directory
            tal_system_sleep(200);
            sdcard_demo_list_dir(SDCARD_MOUNT_PATH);

            // Recursively list all directories
            tal_system_sleep(200);
            sdcard_demo_list_dir_recursive(SDCARD_MOUNT_PATH, 0);

            // Speed test: Write and read 512MB
            tal_system_sleep(500);
            PR_NOTICE("\nStarting SD card speed test (this may take several minutes)...");
            sdcard_demo_speed_test();
        } else {
            PR_ERR("Failed to mount SD card: %d", rt);
        }
    } else {
        PR_WARN("SD card initialization failed: %d (continuing)", rt);
    }

    // Print initial status
    PR_NOTICE("\nInitial Status:");
    print_charge_status();
    print_battery_status();
    print_button_states();
    PR_NOTICE("");

    // Start demo task
    static THREAD_CFG_T thrd_param = {
        .priority = TASK_DEMO_PRIORITY, .stackDepth = TASK_DEMO_SIZE, .thrdname = "eink_nfc_demo"};
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_demo_thrd_hdl, NULL, NULL, demo_task, NULL, &thrd_param));

    PR_NOTICE("Demo application started. Status will be printed every %d seconds.", STATUS_PRINT_INTERVAL / 1000);

    return;
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();

    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
