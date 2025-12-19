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
#include "cJSON.h"
#include "http_download.h"

#if defined(ENABLE_BUTTON) && (ENABLE_BUTTON == 1)
#include "tdl_button_manage.h"
#endif

#if defined(ENABLE_LED) && (ENABLE_LED == 1)
#include "tdl_led_manage.h"
#endif

#include "app_display.h"
#include "ai_audio.h"
#include "app_chat_bot.h"
#include "media_src_zh.h"
#include "app_pwm.h"

// 声明 EPD 显示图片函数
extern void EPD_7in5_display_downloaded_image(uint8_t *jpeg_data, uint32_t jpeg_size);

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
    .is_open = false,
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

// 图片下载相关变量
static uint8_t *s_image_download_buf = NULL;
static uint32_t s_image_download_size = 0;
static uint32_t s_image_download_offset = 0;

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief 图片下载事件回调
 */
static void __image_download_event_cb(http_download_event_id_t id, http_download_event_t *event)
{
    switch (id) {
    case DL_EVENT_ON_FILESIZE:
        PR_DEBUG("Image file size: %d bytes", event->file_size);
        s_image_download_size = event->file_size;
        s_image_download_offset = 0;
        // 分配内存
        if (s_image_download_buf) {
            tkl_system_psram_free(s_image_download_buf);
        }
        s_image_download_buf = tkl_system_psram_malloc(event->file_size);
        if (!s_image_download_buf) {
            PR_ERR("Failed to allocate memory for image download");
        }
        break;
    case DL_EVENT_ON_DATA:
        if (s_image_download_buf && event->data) {
            memcpy(s_image_download_buf + event->offset, event->data, event->data_len);
            s_image_download_offset = event->offset + event->data_len;
            PR_DEBUG("Downloaded %d/%d bytes", s_image_download_offset, s_image_download_size);
        }
        break;
    case DL_EVENT_FINISH:
        PR_INFO("Image download complete: %d bytes", s_image_download_offset);
        // 下载完成，显示图片
        if (s_image_download_buf && s_image_download_offset > 0) {
            EPD_7in5_display_downloaded_image(s_image_download_buf, s_image_download_offset);
        }
        break;
    case DL_EVENT_FAULT:
        PR_ERR("Image download failed");
        if (s_image_download_buf) {
            tkl_system_psram_free(s_image_download_buf);
            s_image_download_buf = NULL;
        }
        break;
    default:
        break;
    }
}

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

// 声明证书查询函数
extern int tuya_iotdns_query_domain_certs(char *url, uint8_t **cacert, uint16_t *cacert_len);

/**
 * @brief 下载图片并显示在墨水屏上
 * @param image_url 图片 URL
 */
static void __download_and_display_image(const char *image_url)
{
    if (image_url == NULL || image_url[0] == '\0') {
        return;
    }

    PR_INFO("Starting image download: %s", image_url);

    // 查询域名证书（用于 HTTPS 连接）
    uint8_t *cert = NULL;
    uint16_t cert_len = 0;
    int cert_ret = tuya_iotdns_query_domain_certs((char *)image_url, &cert, &cert_len);
    if (cert_ret != 0) {
        PR_WARN("Failed to get domain cert: %d, will try without cert verification", cert_ret);
    } else {
        PR_DEBUG("Got domain cert, len: %d", cert_len);
    }

    http_download_config_t config = {
        .url = image_url,
        .cacert = cert,
        .cacert_len = cert_len,
        .timeout_ms = 60000,
        .range_length = 16 * 1024,
        .file_size = 0,
        .user_data = NULL,
        .event_handler = __image_download_event_cb,
    };

    int ret = http_file_download(&config);
    if (ret != 0) {
        PR_ERR("Failed to start image download: %d", ret);
    }

    // 释放证书内存
    if (cert) {
        tal_free(cert);
    }
}
static void __app_ai_audio_evt_inform_cb(AI_AUDIO_EVENT_E event, uint8_t *data, uint32_t len, void *arg)
{
    // 用于收集完整的 AI 回复文本（用于提取图片 URL）
    static uint8_t *p_ai_text = NULL;
    static uint32_t ai_text_len = 0;

    switch (event) {
    case AI_AUDIO_EVT_HUMAN_ASR_TEXT: {
        if (len > 0 && data) {
            // 打印用户输入
            PR_INFO("User ASR: %.*s", len, (char *)data);
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
            app_display_send_msg(TY_DISPLAY_TP_USER_MSG, data, len);
#endif
        }
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_START: {
        // 初始化文本收集缓冲区
        if (NULL == p_ai_text) {
            p_ai_text = tkl_system_psram_malloc(AI_AUDIO_TEXT_BUFF_LEN);
        }
        if (p_ai_text) {
            ai_text_len = 0;
            memset(p_ai_text, 0, AI_AUDIO_TEXT_BUFF_LEN);
        }
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_START, data, len);
#endif
#endif
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_DATA: {
        // 收集 AI 回复文本
        if (p_ai_text && data && len > 0 && ai_text_len + len < AI_AUDIO_TEXT_BUFF_LEN) {
            memcpy(p_ai_text + ai_text_len, data, len);
            ai_text_len += len;
        }
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_DATA, data, len);
#else
        if (ai_text_len >= AI_AUDIO_TEXT_SHOW_LEN) {
            app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG, p_ai_text, ai_text_len);
        }
#endif
#endif
    } break;
    case AI_AUDIO_EVT_AI_REPLIES_TEXT_END: {
        // 追加最后的数据
        if (data && len > 0 && p_ai_text && ai_text_len + len < AI_AUDIO_TEXT_BUFF_LEN) {
            memcpy(p_ai_text + ai_text_len, data, len);
            ai_text_len += len;
        }

        // 打印完整的 AI 回复文本
        if (p_ai_text && ai_text_len > 0) {
            p_ai_text[ai_text_len] = '\0';
            PR_INFO("AI Reply Text: %s", (char *)p_ai_text);

            // 检查是否包含图片 URL
            char image_url[512] = {0};
            if (__extract_image_url_from_text((const char *)p_ai_text, ai_text_len, image_url, sizeof(image_url))) {
                PR_NOTICE("Found image URL in AI response: %s", image_url);
                // 下载并显示图片
                __download_and_display_image(image_url);
            }
        }

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#if defined(ENABLE_GUI_STREAM_AI_TEXT) && (ENABLE_GUI_STREAM_AI_TEXT == 1)
        app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG_STREAM_END, data, len);
#else
        if (p_ai_text && ai_text_len > 0) {
            app_display_send_msg(TY_DISPLAY_TP_ASSISTANT_MSG, p_ai_text, ai_text_len);
        }
#endif
#endif
        // 重置文本长度（保留缓冲区供下次使用）
        ai_text_len = 0;
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

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_EMOTION, (uint8_t *)EMOJI_NEUTRAL, strlen(EMOJI_NEUTRAL));
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)STANDBY, strlen(STANDBY));
#endif
        break;
    case AI_AUDIO_STATE_LISTEN:
#if defined(ENABLE_LED) && (ENABLE_LED == 1)
        tdl_led_set_status(sg_led_hdl, TDL_LED_ON);
#endif

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)LISTENING, strlen(LISTENING));
#endif
        break;
    case AI_AUDIO_STATE_UPLOAD:

        break;
    case AI_AUDIO_STATE_AI_SPEAK:
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
        app_display_send_msg(TY_DISPLAY_TP_STATUS, (uint8_t *)SPEAKING, strlen(SPEAKING));
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

#if defined(ENABLE_BUTTON_2) && (ENABLE_BUTTON_2 == 1)
static void __button_function_cb(char *name, TDL_BUTTON_TOUCH_EVENT_E event, void *argc)
{
    static uint8_t brightness = 0;
    switch (event) {
    case TDL_BUTTON_PRESS_DOWN: {
        PR_NOTICE("%s: single click", name);
        // app_pwm_set_duty(100);
    } break;
    case TDL_BUTTON_LONG_PRESS_START: {
        PR_NOTICE("%s: long press", name);
    } break;
    case TDL_BUTTON_PRESS_SINGLE_CLICK: {
        PR_NOTICE("%s: single click", name);
        app_pwm_set_duty(brightness*100);
        brightness+=10 ;
    } break;
    case TDL_BUTTON_PRESS_DOUBLE_CLICK: {
        PR_NOTICE("%s: double click", name);
        // app_pwm_set_duty(0);
    } break;
    default:
        break;
    }
}
#endif

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

    // TDL_BUTTON_HANDLE button_hdl_2 = NULL;

    // TUYA_CALL_ERR_RETURN(tdl_button_create(BUTTON_NAME_2, &button_cfg, &button_hdl_2));

    // tdl_button_event_register(button_hdl_2, TDL_BUTTON_PRESS_SINGLE_CLICK, __button_function_cb);
    // tdl_button_event_register(button_hdl_2, TDL_BUTTON_PRESS_DOUBLE_CLICK, __button_function_cb);
    return rt;
}
#endif

OPERATE_RET app_chat_bot_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    AI_AUDIO_CONFIG_T ai_audio_cfg;

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

    ai_audio_player_start(alert_id);

    switch (type) {
    case AI_AUDIO_ALERT_POWER_ON: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_prologue_zh, sizeof(media_src_prologue_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NOT_ACTIVE: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conn_zh, sizeof(media_src_network_conn_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_CFG: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_config_zh, sizeof(media_src_network_config_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_CONNECTED: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conn_success_zh,
                                        sizeof(media_src_network_conn_success_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_FAIL: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_conn_failed_zh, sizeof(media_src_network_conn_failed_zh), 1);
    } break;
    case AI_AUDIO_ALERT_NETWORK_DISCONNECT: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_network_reconfigure_zh,
                                        sizeof(media_src_network_reconfigure_zh), 1);
    } break;
    case AI_AUDIO_ALERT_BATTERY_LOW: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_low_battery_zh, sizeof(media_src_low_battery_zh), 1);
    } break;
    case AI_AUDIO_ALERT_PLEASE_AGAIN: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_please_again_zh, sizeof(media_src_please_again_zh), 1);
    } break;
    case AI_AUDIO_ALERT_WAKEUP: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_ai_zh, sizeof(media_src_ai_zh), 1);
    } break;
    case AI_AUDIO_ALERT_LONG_KEY_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_long_press_zh,
                                        sizeof(media_src_long_press_zh), 1);
    } break;
    case AI_AUDIO_ALERT_KEY_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_press_talk_zh, sizeof(media_src_press_talk_zh), 1);
    } break;
    case AI_AUDIO_ALERT_WAKEUP_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_wakeup_chat_zh, sizeof(media_src_wakeup_chat_zh),
                                        1);
    } break;
    case AI_AUDIO_ALERT_FREE_TALK: {
        rt = ai_audio_player_data_write(alert_id, (uint8_t *)media_src_free_chat_zh, sizeof(media_src_free_chat_zh),
                                        1);
    } break;

    default:
        break;
    }

    return rt;
}
