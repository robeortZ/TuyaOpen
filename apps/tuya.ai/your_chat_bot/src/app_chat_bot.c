/**
 * @file app_chat_bot.c
 * @brief app_chat_bot module is used to
 * @version 0.1
 * @date 2025-03-25
 */
#include "netmgr.h"

#include "tkl_wifi.h"
#include "tkl_gpio.h"
#include "tkl_memory.h"
#include "tal_api.h"
#include "tuya_ringbuf.h"

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "tdl_button_manage.h"
#endif

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
#include "tdl_led_manage.h"
#endif

#include "app_display.h"
#include "ai_audio.h"
#include "app_chat_bot.h"
#include "app_chat_history.h"
#include "app_web_server.h"
#include "media_src_zh.h"
#include "cJSON.h"
/***********************************************************
************************macro define************************
***********************************************************/
#define AI_AUDIO_TEXT_BUFF_LEN (1024)
#define AI_AUDIO_TEXT_SHOW_LEN (60 * 3)

typedef uint8_t APP_CHAT_MODE_E;
/*Press and hold button to start a single conversation.*/
#define APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE 0
/*Press the button once to start or stop the free conversation.*/
#define APP_CHAT_MODE_KEY_TRIG_VAD_FREE 1
/*Say the wake-up word to start a single conversation, similar to a smart speaker.
 *If no conversation is detected within 20 seconds, you need to say the wake-up word again*/
#define APP_CHAT_MODE_ASR_WAKEUP_SINGLE 2
/*Saying the wake-up word, you can have a free conversation.
 *If no conversation is detected within 20 seconds, you need to say the wake-up word again*/
#define APP_CHAT_MODE_ASR_WAKEUP_FREE 3

#define APP_CHAT_MODE_MAX 4
/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef struct {
    APP_CHAT_MODE_E mode;
    AI_AUDIO_WORK_MODE_E auido_mode;
    AI_AUDIO_ALERT_TYPE_E mode_alert;
    char *display_text;
    bool is_open;
} CHAT_WORK_MODE_INFO_T;

typedef struct {
    uint8_t is_enable;
    const CHAT_WORK_MODE_INFO_T *work;
} APP_CHAT_BOT_S;

/***********************************************************
***********************const declaration********************
***********************************************************/
const CHAT_WORK_MODE_INFO_T cAPP_WORK_HOLD = {
    .mode = APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE,
    .auido_mode = AI_AUDIO_MODE_MANUAL_SINGLE_TALK,
    .mode_alert = AI_AUDIO_ALERT_LONG_KEY_TALK,
    .display_text = HOLD_TALK,
    .is_open = true,
};

const CHAT_WORK_MODE_INFO_T cAPP_WORK_TRIG_VAD = {
    .mode = APP_CHAT_MODE_KEY_TRIG_VAD_FREE,
    .auido_mode = AI_AUDIO_WORK_VAD_FREE_TALK,
    .mode_alert = AI_AUDIO_ALERT_KEY_TALK,
    .display_text = TRIG_TALK,
    .is_open = true,  // Enable VAD free talk by default
};

const CHAT_WORK_MODE_INFO_T cAPP_WORK_WAKEUP_SINGLE = {
    .mode = APP_CHAT_MODE_ASR_WAKEUP_SINGLE,
    .auido_mode = AI_AUDIO_WORK_ASR_WAKEUP_SINGLE_TALK,
    .mode_alert = AI_AUDIO_ALERT_WAKEUP_TALK,
    .display_text = WAKEUP_TALK,
    .is_open = true,
};

const CHAT_WORK_MODE_INFO_T cAPP_WORK_WAKEUP_FREE = {
    .mode = APP_CHAT_MODE_ASR_WAKEUP_FREE,
    .auido_mode = AI_AUDIO_WORK_ASR_WAKEUP_FREE_TALK,
    .mode_alert = AI_AUDIO_ALERT_FREE_TALK,
    .display_text = FREE_TALK,
    .is_open = true,
};

#if 0
const CHAT_WORK_MODE_INFO_T *cWORK_MODE_INFO_LIST[] = {
    &cAPP_WORK_HOLD,
    &cAPP_WORK_TRIG_VAD,
    &cAPP_WORK_WAKEUP_SINGLE,
    &cAPP_WORK_WAKEUP_FREE,
};
#endif
/***********************************************************
***********************variable define**********************
***********************************************************/
static APP_CHAT_BOT_S sg_chat_bot = {
#if defined(ENABLE_CHAT_MODE_KEY_PRESS_HOLD_SINGEL) && (ENABLE_CHAT_MODE_KEY_PRESS_HOLD_SINGEL == 1)
    .work = &cAPP_WORK_HOLD,
#endif

#if defined(ENABLE_CHAT_MODE_KEY_TRIG_VAD_FREE) && (ENABLE_CHAT_MODE_KEY_TRIG_VAD_FREE == 1)
    .work = &cAPP_WORK_TRIG_VAD,
#endif

#if defined(ENABLE_CHAT_MODE_ASR_WAKEUP_SINGEL) && (ENABLE_CHAT_MODE_ASR_WAKEUP_SINGEL == 1)
    .work = &cAPP_WORK_WAKEUP_SINGLE,
#endif

#if defined(ENABLE_CHAT_MODE_ASR_WAKEUP_FREE) && (ENABLE_CHAT_MODE_ASR_WAKEUP_FREE == 1)
    .work = &cAPP_WORK_WAKEUP_FREE,
#endif

};

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
static TDL_LED_HANDLE_T sg_led_hdl = NULL;
#endif

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
static TDL_BUTTON_HANDLE sg_button_hdl = NULL;
#endif

#if defined(ENABLE_KEYBOARD_INPUT) && (ENABLE_KEYBOARD_INPUT == 1)
// State tracking for keyboard (simulates press/hold behavior)
static bool s_keyboard_listening = false;
// Flag for continuous conversation mode (auto-restart listening after AI response)
static bool s_continuous_conversation = false;
#endif

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Extract image URL from text that may contain JSON array
 * @param text Text content that may contain JSON array with imageUrl
 * @param text_len Length of text
 * @param image_url Output buffer for image URL
 * @param url_buf_size Size of image_url buffer
 * @return true if image URL found, false otherwise
 */
static bool __extract_image_url_from_text(const char *text, uint32_t text_len, char *image_url, uint32_t url_buf_size)
{
    if (text == NULL || text_len == 0 || image_url == NULL || url_buf_size == 0) {
        return false;
    }

    // Find JSON array in text (format: [{"imageUrl":"..."}])
    const char *json_start = strchr(text, '[');
    if (json_start == NULL) {
        return false;
    }

    // Find matching closing bracket
    const char *json_end = strrchr(json_start, ']');
    if (json_end == NULL) {
        return false;
    }

    // Extract JSON substring
    uint32_t json_len = json_end - json_start + 1;
    char *json_str = tal_malloc(json_len + 1);
    if (json_str == NULL) {
        return false;
    }
    memcpy(json_str, json_start, json_len);
    json_str[json_len] = '\0';

    // Parse JSON
    cJSON *json = cJSON_Parse(json_str);
    tal_free(json_str);
    if (json == NULL) {
        return false;
    }

    // Check if it's an array
    if (!cJSON_IsArray(json)) {
        cJSON_Delete(json);
        return false;
    }

    // Get first element
    cJSON *first_item = cJSON_GetArrayItem(json, 0);
    if (first_item == NULL) {
        cJSON_Delete(json);
        return false;
    }

    // Extract imageUrl field
    cJSON *image_url_item = cJSON_GetObjectItem(first_item, "imageUrl");
    if (image_url_item == NULL || !cJSON_IsString(image_url_item)) {
        cJSON_Delete(json);
        return false;
    }

    const char *url = cJSON_GetStringValue(image_url_item);
    if (url != NULL && url[0] != '\0') {
        uint32_t url_len = strlen(url);
        uint32_t copy_len = (url_len < url_buf_size - 1) ? url_len : url_buf_size - 1;
        memcpy(image_url, url, copy_len);
        image_url[copy_len] = '\0';
        cJSON_Delete(json);
        PR_DEBUG("Extracted image URL from text: %s", image_url);
        return true;
    }

    cJSON_Delete(json);
    return false;
}

static void __app_ai_audio_evt_inform_cb(AI_AUDIO_EVENT_E event, uint8_t *data, uint32_t len, void *arg)
{
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#if !defined(ENABLE_GUI_STREAM_AI_TEXT) || (ENABLE_GUI_STREAM_AI_TEXT != 1)
    static uint8_t *p_ai_text = NULL;
    static uint32_t ai_text_len = 0;
#else
    // Buffer for collecting streamed AI text
    static uint8_t *p_ai_stream_text = NULL;
    static uint32_t ai_stream_text_len = 0;
#endif
#else
    // Buffer for collecting AI text in non-display mode
    static uint8_t *p_ai_text = NULL;
    static uint32_t ai_text_len = 0;
#endif

    switch (event) {
    case AI_AUDIO_EVT_HUMAN_ASR_TEXT: {
        if (len > 0 && data) {
// send asr text to display
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
            app_display_send_msg(TY_DISPLAY_TP_USER_MSG, data, len);
#else
            // Ubuntu console logging
            PR_NOTICE("USER: %.*s", (int)len, data);
#endif
            // Save user message to chat history
            app_chat_history_add_message(CHAT_MSG_TYPE_USER, (const char *)data, len);
        }
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_START: {
        // Start streaming message in chat history
        app_chat_history_start_streaming_message(CHAT_MSG_TYPE_ASSISTANT);
        
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_START, data, len);
        // Initialize stream buffer
        if (NULL == p_ai_stream_text) {
            p_ai_stream_text = tkl_system_psram_malloc(AI_AUDIO_TEXT_BUFF_LEN);
            if (NULL == p_ai_stream_text) {
                return;
            }
        }
        ai_stream_text_len = 0;
#else
        if (NULL == p_ai_text) {
            p_ai_text = tkl_system_psram_malloc(AI_AUDIO_TEXT_BUFF_LEN);
            if (NULL == p_ai_text) {
                return;
            }
        }

        ai_text_len = 0;
#endif
#else
        // Ubuntu console logging - AI response start
        PR_NOTICE("AI: ", len);
        // Initialize buffer for non-display mode
        if (NULL == p_ai_text) {
            p_ai_text = tkl_system_psram_malloc(AI_AUDIO_TEXT_BUFF_LEN);
            if (NULL == p_ai_text) {
                return;
            }
        }
        ai_text_len = 0;
#endif
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_DATA: {
        // Check for valid data before any processing
        if (data == NULL || len == 0) {
            break;
        }
        
        // Update chat history with streaming text in real-time
        app_chat_history_append_to_last_message((const char *)data, len);
        
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_DATA, data, len);
        // Collect streamed text
        if (p_ai_stream_text && ai_stream_text_len + len < AI_AUDIO_TEXT_BUFF_LEN) {
            memcpy(p_ai_stream_text + ai_stream_text_len, data, len);
            ai_stream_text_len += len;
        }
#else
        if (p_ai_text) {
            memcpy(p_ai_text + ai_text_len, data, len);
            ai_text_len += len;
            if (ai_text_len >= AI_AUDIO_TEXT_SHOW_LEN) {
                app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG, p_ai_text, ai_text_len);
                ai_text_len = 0;
            }
        }
#endif
#else
        PR_NOTICE("AI: %.*s", (int)len, data);
        // Collect AI text in non-display mode
        if (p_ai_text && ai_text_len + len < AI_AUDIO_TEXT_BUFF_LEN) {
            memcpy(p_ai_text + ai_text_len, data, len);
            ai_text_len += len;
        }
#endif
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_END: {
        // Append any remaining data first
        if (data && len > 0) {
            app_chat_history_append_to_last_message((const char *)data, len);
        }
        
        // Check if the complete message contains image URL in JSON format
        char last_msg_text[CHAT_MESSAGE_MAX_LEN] = {0};
        if (app_chat_history_get_last_message_text(last_msg_text, sizeof(last_msg_text)) == OPRT_OK) {
            char image_url[512] = {0};
            if (__extract_image_url_from_text(last_msg_text, strlen(last_msg_text), image_url, sizeof(image_url))) {
                PR_NOTICE("Extracted image URL: %s", image_url);
                // Add image URL to message
                app_chat_history_add_image_to_last_message(image_url);
                
                // Remove JSON part from text, keep only the prefix text (e.g., "图片生成成功")
                const char *json_start = strchr(last_msg_text, '[');
                if (json_start != NULL) {
                    // Truncate text at JSON start
                    uint32_t prefix_len = json_start - last_msg_text;
                    // Remove trailing spaces
                    while (prefix_len > 0 && (last_msg_text[prefix_len - 1] == ' ' || last_msg_text[prefix_len - 1] == '\t')) {
                        prefix_len--;
                    }
                    last_msg_text[prefix_len] = '\0';
                    // Update the message text
                    app_chat_history_update_last_message_text(last_msg_text, prefix_len);
                    PR_DEBUG("Cleaned text, removed JSON part");
                }
                
                // Stop audio playback to prevent reading URL
                ai_audio_player_stop();
            }
        }
        
        // Message is now complete - end streaming to remove cursor
        app_chat_history_end_streaming();
        
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_END, data, len);
        // Clear stream buffer
        if (p_ai_stream_text) {
            ai_stream_text_len = 0;
        }
#else
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG, p_ai_text, ai_text_len);
        // Clear buffer
        if (p_ai_text) {
            ai_text_len = 0;
        }
#endif
#else
        // Clear buffer for non-display mode
        if (p_ai_text) {
            ai_text_len = 0;
        }
#endif
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_INTERUPT: {
        // End streaming on interrupt to remove cursor
        app_chat_history_end_streaming();
#if defined(ENABLE_KEYBOARD_INPUT) && (ENABLE_KEYBOARD_INPUT == 1)
        // Server VAD detected speech end, update keyboard listening state
        if (s_keyboard_listening) {
            s_keyboard_listening = false;
            PR_NOTICE("Server VAD: Stopped listening");
        }
#endif
#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_INTERRUPT, NULL, 0);
#else
        PR_WARN("AI response interrupted");
#endif
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_EMO: {
        AI_AUDIO_EMOTION_T *emo;
        PR_DEBUG("---> AI_MSG_TYPE_EMOTION");
        emo = (AI_AUDIO_EMOTION_T *)data;
        if (emo) {
            if (emo->name) {
                PR_DEBUG("emotion name:%s", emo->name);
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
                app_display_send_msg(TY_DISPLAY_TP_EMOTION, (uint8_t *)emo->name, strlen(emo->name));
#endif
            }

            if (emo->text) {
                PR_DEBUG("emotion text:%s", emo->text);
            }
        }
    } break;
    case AI_AUDIO_EVT_ASR_WAKEUP: {
        ai_audio_player_stop();
        ai_audio_player_play_alert(AI_AUDIO_ALERT_WAKEUP);

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
        TDL_LED_BLINK_CFG_T blink_cfg = {
            .cnt = 2,
            .start_stat = TDL_LED_ON,
            .end_stat = TDL_LED_OFF,
            .first_half_cycle_time = 100,
            .latter_half_cycle_time = 100,
        };

        tdl_led_blink(sg_led_hdl, &blink_cfg);
#endif

#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_END, data, len);
#endif
    } break;

    default:
        break;
    }

    return;
}

static void __app_ai_audio_state_inform_cb(AI_AUDIO_STATE_E state)
{

    PR_DEBUG("ai audio state: %d", state);

    switch (state) {
    case AI_AUDIO_STATE_STANDBY:

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
        tdl_led_set_status(sg_led_hdl, TDL_LED_OFF);
#endif

#if defined(ENABLE_KEYBOARD_INPUT) && (ENABLE_KEYBOARD_INPUT == 1)
        // Reset keyboard listening state when entering STANDBY
        s_keyboard_listening = false;
        
        // If in continuous conversation mode, auto-restart listening after a short delay
        if (s_continuous_conversation) {
            PR_NOTICE("Continuous mode: Auto-restart listening...");
            // Small delay to let audio finish
            tal_system_sleep(500);
            // Play alert and start listening again
            ai_audio_player_play_alert(AI_AUDIO_ALERT_WAKEUP);
            ai_audio_manual_start_single_talk();
            s_keyboard_listening = true;
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
            tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
#endif
            PR_NOTICE("State: STANDBY -> Auto listening (press X to stop)");
            break;
        }
#endif

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_EMOTION, (uint8_t *)EMOJI_NEUTRAL, strlen(EMOJI_NEUTRAL));
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)STANDBY, strlen(STANDBY));
#else
        PR_NOTICE("State: STANDBY (Ready for next conversation)");
#endif
        break;
    case AI_AUDIO_STATE_LISTEN:
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
        tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
#endif

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)LISTENING, strlen(LISTENING));
#else
        PR_NOTICE("State: LISTENING (Recording audio...)");
#endif
        break;
    case AI_AUDIO_STATE_UPLOAD:
#if !defined(ENABLE_CHAT_DISPLAY) || (ENABLE_CHAT_DISPLAY != 1)
        PR_NOTICE("State: UPLOAD (Sending to cloud...)");
#endif
        break;
    case AI_AUDIO_STATE_AI_SPEAK:
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)SPEAKING, strlen(SPEAKING));
#else
        PR_NOTICE("State: AI_SPEAKING (Playing response...)");
#endif

        break;

    default:
        break;
    }
}

static OPERATE_RET __app_chat_bot_enable(uint8_t enable)
{
    if (sg_chat_bot.is_enable == enable) {
        PR_DEBUG("chat bot enable is already %s", enable ? "enable" : "disable");
        return OPRT_OK;
    }

    PR_DEBUG("chat bot enable set %s", enable ? "enable" : "disable");

    ai_audio_set_open(enable);

    sg_chat_bot.is_enable = enable;

    return OPRT_OK;
}

uint8_t app_chat_bot_get_enable(void)
{
    return sg_chat_bot.is_enable;
}

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
static void __app_button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    APP_CHAT_MODE_E work_mode = sg_chat_bot.work->mode;
    PR_DEBUG("app button function cb, work mode: %d", work_mode);

    // network status
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    if (status == NETMGR_LINK_DOWN) {
        PR_DEBUG("network is down, ignore button event");
        if (ai_audio_player_is_playing()) {
            return;
        }
        ai_audio_player_play_alert(AI_AUDIO_ALERT_NOT_ACTIVE);
        return;
    }

    switch (event) {
    case TDL_BUTTON_PRESS_DOWN: {
        if (work_mode == APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE) {
            PR_DEBUG("button press down, listen start");
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
            tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
#endif
            ai_audio_manual_start_single_talk();
        }
    } break;
    case TDL_BUTTON_PRESS_UP: {
        if (work_mode == APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE) {
            PR_DEBUG("button press up, listen end");
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
            tdl_led_set_status(sg_led_hdl, TDL_LED_OFF);
#endif
            ai_audio_manual_stop_single_talk();
        }
    } break;
    case TDL_BUTTON_PRESS_SINGLE_CLICK: {
        if (work_mode == APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE) {
            break;
        }

        if (sg_chat_bot.is_enable) {
            ai_audio_player_stop();
            ai_audio_player_play_alert(AI_AUDIO_ALERT_WAKEUP);
            ai_audio_set_wakeup();
        } else {
            __app_chat_bot_enable(true);
        }
        PR_DEBUG("button single click");
    } break;
    default:
        break;
    }
}

static OPERATE_RET __app_open_button(void)
{
    OPERATE_RET rt = OPRT_OK;

    TDL_BUTTON_CFG_T button_cfg = {.long_start_valid_time = 3000,
                                   .long_keep_timer = 1000,
                                   .button_debounce_time = 50,
                                   .button_repeat_valid_count = 2,
                                   .button_repeat_valid_time = 500};
    TUYA_CALL_ERR_RETURN(tdl_button_create(BUTTON_NAME, &button_cfg, &sg_button_hdl));

    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOWN, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_UP, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_SINGLE_CLICK, __app_button_function_cb);
    tdl_button_event_register(sg_button_hdl, TDL_BUTTON_PRESS_DOUBLE_CLICK, __app_button_function_cb);

    return rt;
}
#endif

#if defined(ENABLE_KEYBOARD_INPUT) && (ENABLE_KEYBOARD_INPUT == 1)
#include "tuya_ai_client.h"

/**
 * @brief Keyboard event handler
 *
 * Handles keyboard events from the board layer.
 * Maps keyboard keys to chatbot functionality:
 * - S: Start listening / Trigger wakeup
 * - X: Stop listening
 * - V: Volume up
 * - D: Volume down
 * - Q: Quit (handled in keyboard_input.c)
 */
void app_chat_bot_keyboard_event_handler(KEYBOARD_EVENT_E event)
{
    APP_CHAT_MODE_E work_mode = sg_chat_bot.work->mode;

    switch (event) {
    case KEYBOARD_EVENT_PRESS_S: {
        PR_DEBUG("Keyboard 'S' pressed, work_mode: %d", work_mode);

        // Check if AI client is ready
        if (!tuya_ai_client_is_ready()) {
            PR_WARN("AI client not ready, please wait for connection");
            if (!ai_audio_player_is_playing()) {
                ai_audio_player_play_alert(AI_AUDIO_ALERT_NOT_ACTIVE);
            }
            return;
        }

        // Handle based on work mode
        if (work_mode == APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE) {
            // Toggle mode: S key toggles listening on/off
            if (s_keyboard_listening) {
                // Currently listening -> stop and send to AI (stays in continuous mode)
                PR_NOTICE("Keyboard: Stop listening and send to AI");
                s_keyboard_listening = false;
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
                tdl_led_set_status(sg_led_hdl, TDL_LED_OFF);
#endif
                ai_audio_manual_stop_single_talk();
                // Note: s_continuous_conversation stays true, will auto-restart after AI responds
            } else {
                // Not listening -> start new conversation session
                if (ai_audio_player_is_playing()) {
                    ai_audio_player_stop();
                }
                
                // Stop any previous activity
                ai_audio_manual_stop_single_talk();
                ai_audio_cloud_asr_stop();
                ai_audio_agent_upload_stop();
                
                ai_audio_player_play_alert(AI_AUDIO_ALERT_WAKEUP);
                ai_audio_manual_start_single_talk();
                
                // Enable continuous conversation mode
                s_continuous_conversation = true;
                s_keyboard_listening = true;
                PR_NOTICE("Keyboard: Start continuous conversation (press S to send, X to end session)");
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
                tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
#endif
            }
        } else if (work_mode == APP_CHAT_MODE_KEY_TRIG_VAD_FREE) {
            // VAD mode (may not work on Ubuntu platform)
            if (ai_audio_player_is_playing()) {
                ai_audio_player_stop();
            }
            ai_audio_player_play_alert(AI_AUDIO_ALERT_WAKEUP);
            ai_audio_set_wakeup();
            PR_NOTICE("Keyboard: Trigger VAD mode");
        }
        break;
    }

    case KEYBOARD_EVENT_PRESS_X: {
        PR_DEBUG("Keyboard 'X' pressed, work_mode: %d", work_mode);

        if (work_mode == APP_CHAT_MODE_KEY_PRESS_HOLD_SINGLE) {
            // X key ends the entire conversation session
            PR_NOTICE("Keyboard: End conversation session");
            s_continuous_conversation = false;  // Exit continuous mode
            s_keyboard_listening = false;
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
            tdl_led_set_status(sg_led_hdl, TDL_LED_OFF);
#endif
            // Stop all audio activities
            ai_audio_player_stop();
            ai_audio_manual_stop_single_talk();
            ai_audio_cloud_asr_stop();
            ai_audio_agent_upload_stop();
        } else if (work_mode == APP_CHAT_MODE_KEY_TRIG_VAD_FREE) {
            // Stop VAD free conversation
            ai_audio_player_stop();
            ai_audio_cloud_asr_stop();
            ai_audio_agent_upload_stop();
            PR_NOTICE("Keyboard: Stop free conversation");
        }
        break;
    }

    case KEYBOARD_EVENT_PRESS_V: {
        // Volume up
        uint8_t volume = ai_audio_get_volume();
        if (volume < 100) {
            volume = (volume + 10 > 100) ? 100 : volume + 10;
            ai_audio_set_volume(volume);
            PR_NOTICE("Volume increased to %d%%", volume);
        }
        break;
    }

    case KEYBOARD_EVENT_PRESS_D: {
        // Volume down
        uint8_t volume = ai_audio_get_volume();
        if (volume > 0) {
            volume = (volume < 10) ? 0 : volume - 10;
            ai_audio_set_volume(volume);
            PR_NOTICE("Volume decreased to %d%%", volume);
        }
        break;
    }

    case KEYBOARD_EVENT_PRESS_Q:
        PR_NOTICE("Quit requested via keyboard");
        // Quit is handled in keyboard_input.c
        break;

    default:
        break;
    }
}
#endif

OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_AUDIO_CONFIG_T ai_audio_cfg;

    // Initialize chat history module
    TUYA_CALL_ERR_RETURN(app_chat_history_init());

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
    app_display_init();
#endif

    ai_audio_cfg.work_mode = sg_chat_bot.work->auido_mode;
    ai_audio_cfg.evt_inform_cb = __app_ai_audio_evt_inform_cb;
    ai_audio_cfg.state_inform_cb = __app_ai_audio_state_inform_cb;

    TUYA_CALL_ERR_RETURN(ai_audio_init(&ai_audio_cfg));

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
    TUYA_CALL_ERR_RETURN(__app_open_button());
#endif

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
    sg_led_hdl = tdl_led_find_dev(LED_NAME);
    TUYA_CALL_ERR_RETURN(tdl_led_open(sg_led_hdl));
#endif

    __app_chat_bot_enable(sg_chat_bot.work->is_open);

    PR_NOTICE("work:%s", sg_chat_bot.work->display_text);

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
    app_display_send_msg(TY_DISPLAY_TP_CHAT_MODE, (uint8_t *)sg_chat_bot.work->display_text,
                         strlen(sg_chat_bot.work->display_text));
#endif

    // Initialize and start web server for chat history display
    rt = app_web_server_init();
    if (rt != OPRT_OK) {
        PR_WARN("Failed to initialize web server: %d", rt);
        // Continue even if web server fails
    }

    return OPRT_OK;
}

/**
 * @brief Plays an alert sound based on the specified alert type.
 *
 * @param type - The type of alert to play, defined by the APP_ALERT_TYPE_E enum.
 * @return OPERATE_RET - Returns OPRT_OK if the alert sound is successfully played, otherwise returns an error code.
 */
OPERATE_RET ai_audio_player_play_alert(AI_AUDIO_ALERT_TYPE_E type)
{
    OPERATE_RET rt = OPRT_OK;
    char alert_id[64] = {0};

    snprintf(alert_id, sizeof(alert_id), "alert_%d", type);

    rt = ai_audio_player_start(alert_id);

    switch (type) {
    case AI_AUDIO_ALERT_POWER_ON: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_prologue_zh, sizeof(media_src_prologue_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NOT_ACTIVE: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conn_zh,
                                        sizeof(media_src_network_conn_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_CFG: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_config_zh,
                                        sizeof(media_src_network_config_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_CONNECTED: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conn_success_zh,
                                        sizeof(media_src_network_conn_success_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_FAIL: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conn_failed_zh,
                                        sizeof(media_src_network_conn_failed_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_DISCONNECT: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_reconfigure_zh,
                                        sizeof(media_src_network_reconfigure_zh), 1);
    } break;
    case AI_AUDIO_ALERT_BATTERY_LOW: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_low_battery_zh, sizeof(media_src_low_battery_zh),
                                        1);
    } break;
    case AI_AUDIO_ALERT_PLEASE_AGAIN: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_please_again_zh,
                                        sizeof(media_src_please_again_zh), 1);
    } break;
    case AI_AUDIO_ALERT_WAKEUP: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_ai_zh, sizeof(media_src_ai_zh), 1);
    } break;
    case AI_AUDIO_ALERT_LONG_KEY_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_long_press_zh, sizeof(media_src_long_press_zh),
                                        1);
    } break;
    case AI_AUDIO_ALERT_KEY_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_press_talk_zh, sizeof(media_src_press_talk_zh),
                                        1);
    } break;
    case AI_AUDIO_ALERT_WAKEUP_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_wakeup_chat_zh, sizeof(media_src_wakeup_chat_zh),
                                        1);
    } break;
    case AI_AUDIO_ALERT_FREE_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_free_chat_zh, sizeof(media_src_free_chat_zh), 1);
    } break;

    default:
        break;
    }

    return rt;
}
