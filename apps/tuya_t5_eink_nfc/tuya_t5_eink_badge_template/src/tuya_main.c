/**
 * @file main.c
 * @brief Main application entry point for badge template
 *
 * This is a template application for developers to start their projects.
 * It provides:
 * - Hardware initialization (SD card, buttons, display)
 * - LVGL initialization
 * - Button input as LVGL keypad (6 buttons: UP, DOWN, LEFT, RIGHT, ENTER, RETURN)
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tal_log.h"
#include "tkl_output.h"
#include "app_hardware.h"
#include "app_lvgl.h"
#include "app_input.h"
#include "app_sdcard_demo.h"
#include "lv_vendor.h"
#include <stdio.h>
#include <stdbool.h>

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "ui.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static lv_obj_t *sg_key_status_label      = NULL;
static lv_obj_t *sg_sdcard_status_label   = NULL;
static lv_obj_t *sg_sdcard_progress_label = NULL;
static lv_obj_t *sg_sdcard_result_label   = NULL;

/***********************************************************
********************function declaration********************
***********************************************************/
static void create_demo_screen(void);
static void update_key_status_text(const char *key_name, bool pressed);
static void update_sdcard_status(const char *status_text);
static void update_sdcard_progress(const char *step_name, uint32_t percent);
static void update_sdcard_result(const char *test_name, float write_speed, float read_speed);

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief Update key status label text
 */
static __attribute__((unused)) void update_key_status_text(const char *key_name, bool pressed)
{
    if (sg_key_status_label == NULL) {
        return;
    }

    char status_text[64];
    if (pressed) {
        snprintf(status_text, sizeof(status_text), "Key: %s\nStatus: PRESSED", key_name);
    } else {
        snprintf(status_text, sizeof(status_text), "Key: %s\nStatus: RELEASED", key_name);
    }

    // Lock display before updating UI (thread safety)
    lv_vendor_disp_lock();
    lv_label_set_text(sg_key_status_label, status_text);
    lv_vendor_disp_unlock();
}

/**
 * @brief Update SD card status on UI
 */
static __attribute__((unused)) void update_sdcard_status(const char *status_text)
{
    if (sg_sdcard_status_label == NULL || status_text == NULL) {
        return;
    }

    // Lock display before updating UI (thread safety)
    lv_vendor_disp_lock();
    lv_label_set_text(sg_sdcard_status_label, status_text);
    lv_vendor_disp_unlock();
}

/**
 * @brief Update SD card progress on UI
 */
static __attribute__((unused)) void update_sdcard_progress(const char *step_name, uint32_t percent)
{
    if (sg_sdcard_progress_label == NULL) {
        return;
    }

    char progress_text[128];
    if (step_name != NULL) {
        snprintf(progress_text, sizeof(progress_text), "%s: %lu%%", step_name, (unsigned long)percent);
    } else {
        snprintf(progress_text, sizeof(progress_text), "Progress: %lu%%", (unsigned long)percent);
    }

    // Lock display before updating UI (thread safety)
    lv_vendor_disp_lock();
    lv_label_set_text(sg_sdcard_progress_label, progress_text);
    lv_vendor_disp_unlock();
}

/**
 * @brief Update SD card test results on UI
 */
static __attribute__((unused)) void update_sdcard_result(const char *test_name, float write_speed, float read_speed)
{
    if (sg_sdcard_result_label == NULL) {
        return;
    }

    char result_text[256];
    if (test_name != NULL) {
        snprintf(result_text, sizeof(result_text), "%s\nWrite: %.2f MB/s\nRead: %.2f MB/s", test_name, write_speed,
                 read_speed);
    } else {
        snprintf(result_text, sizeof(result_text), "Write: %.2f MB/s\nRead: %.2f MB/s", write_speed, read_speed);
    }

    // Lock display before updating UI (thread safety)
    lv_vendor_disp_lock();
    lv_label_set_text(sg_sdcard_result_label, result_text);
    lv_vendor_disp_unlock();
}

/**
 * @brief Create a simple demo screen to test LVGL and input
 */
static __attribute__((unused)) void create_demo_screen(void)
{
    // Create a title label
    lv_obj_t *title_label = lv_label_create(lv_scr_act());
    lv_label_set_text(title_label, "Badge Template");
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    // Create SD card status label
    sg_sdcard_status_label = lv_label_create(lv_scr_act());
    if (app_sdcard_is_mounted()) {
        const char *mount_path = app_sdcard_get_mount_path();
        char        status[128];
        snprintf(status, sizeof(status), "SD Card: %s\nStatus: Mounted", mount_path);
        lv_label_set_text(sg_sdcard_status_label, status);
    } else {
        lv_label_set_text(sg_sdcard_status_label, "SD Card: Not Mounted");
    }
    lv_obj_set_style_text_align(sg_sdcard_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sg_sdcard_status_label, LV_ALIGN_TOP_MID, 0, 40);

    // Create SD card progress label
    sg_sdcard_progress_label = lv_label_create(lv_scr_act());
    lv_label_set_text(sg_sdcard_progress_label, "Progress: Waiting...");
    lv_obj_set_style_text_align(sg_sdcard_progress_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sg_sdcard_progress_label, LV_ALIGN_TOP_MID, 0, 80);

    // Create SD card result label
    sg_sdcard_result_label = lv_label_create(lv_scr_act());
    lv_label_set_text(sg_sdcard_result_label, "Test Results:\nWaiting...");
    lv_obj_set_style_text_align(sg_sdcard_result_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sg_sdcard_result_label, LV_ALIGN_TOP_MID, 0, 120);

    // Create key status label that will be updated on button press/release
    sg_key_status_label = lv_label_create(lv_scr_act());
    lv_label_set_text(sg_key_status_label, "Key: None\nStatus: RELEASED");
    lv_obj_set_style_text_align(sg_key_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sg_key_status_label, LV_ALIGN_BOTTOM_MID, 0, -60);

    // Create instruction label
    lv_obj_t *info_label = lv_label_create(lv_scr_act());
    lv_label_set_text(info_label, "Buttons:\nUP/DOWN/LEFT/RIGHT\nENTER/RETURN");
    lv_obj_set_style_text_align(info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    PR_NOTICE("Demo screen created");
}

/**
 * @brief Main application initialization
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Initialize logging
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("========================================");
    PR_NOTICE("Tuya T5 E-Ink Badge Template");
    PR_NOTICE("========================================");
    PR_NOTICE("Project: %s", PROJECT_NAME);
    PR_NOTICE("Version: %s", PROJECT_VERSION);
    PR_NOTICE("Board: %s", PLATFORM_BOARD);

    // Initialize hardware (buttons, LED, display, SD card)
    rt = app_hardware_init();
    if (OPRT_OK != rt) {
        PR_ERR("Hardware initialization failed: %d", rt);
        return;
    }

    // Initialize LVGL display
    rt = app_lvgl_init();
    if (OPRT_OK != rt) {
        PR_ERR("LVGL initialization failed: %d", rt);
        return;
    }

    // Initialize button input as LVGL keypad
    // Note: This must be called after lv_vendor_init() but before lv_vendor_start()
    rt = app_input_init();
    if (OPRT_OK != rt) {
        PR_ERR("Input initialization failed: %d", rt);
        return;
    }

    // Register callback for key status updates
    app_input_set_status_callback(update_key_status_text);


    // Initialize UI
    lv_vendor_disp_lock();
    ui_init();
    lv_vendor_disp_unlock();

    // Start LVGL task handler (this runs lv_task_handler() in a separate thread)
    app_lvgl_start(4, 4096);

    PR_NOTICE("Badge template initialized successfully");
    PR_NOTICE("You can now start building your application!");

    // Main loop - keep thread alive
    // LVGL runs in its own thread via lv_vendor_start(), but we keep this thread alive
    while (1) {
        tal_system_sleep(1000);
    }
}

/**
 * @brief Main entry point (Linux)
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
 * @brief Application thread
 */
static void tuya_app_thread(void *arg)
{
    (void)arg;
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

/**
 * @brief Tuya application main entry point
 */
void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main",1};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
