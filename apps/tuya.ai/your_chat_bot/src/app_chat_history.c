/**
 * @file app_chat_history.c
 * @brief Chat history storage and management implementation
 * @version 0.1
 * @date 2025-12-05
 */

#include "app_chat_history.h"
#include "tal_api.h"
#include "tal_log.h"
#include "tal_time_service.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

/***********************************************************
***********************variable define**********************
***********************************************************/
static CHAT_MESSAGE_T sg_chat_history[CHAT_HISTORY_MAX_MESSAGES];
static uint32_t sg_message_count = 0;
static uint32_t sg_write_index = 0;
static bool sg_initialized = false;
static uint32_t sg_version = 0; // Version number, increments on any change
static bool sg_is_streaming = false; // Whether AI is currently streaming a response

/***********************************************************
***********************function define**********************
***********************************************************/

OPERATE_RET app_chat_history_init(void)
{
    if (sg_initialized) {
        return OPRT_OK;
    }

    memset(sg_chat_history, 0, sizeof(sg_chat_history));
    sg_message_count = 0;
    sg_write_index = 0;
    sg_initialized = true;

    PR_DEBUG("Chat history module initialized");
    return OPRT_OK;
}

OPERATE_RET app_chat_history_add_message(CHAT_MSG_TYPE_E type, const char *text, uint32_t len)
{
    if (!sg_initialized) {
        app_chat_history_init();
    }

    if (text == NULL || len == 0) {
        return OPRT_INVALID_PARM;
    }

    // Limit text length
    uint32_t copy_len = len;
    if (copy_len >= CHAT_MESSAGE_MAX_LEN) {
        copy_len = CHAT_MESSAGE_MAX_LEN - 1;
    }

    // Get current timestamp
    TIME_T posix_time = tal_time_get_posix();
    uint32_t timestamp = (uint32_t)posix_time;
    if (timestamp == 0) {
        // Fallback to standard time() if tal_time_get_posix() fails
        timestamp = (uint32_t)time(NULL);
    }

    // Add message to circular buffer
    CHAT_MESSAGE_T *msg = &sg_chat_history[sg_write_index];
    msg->timestamp = timestamp;
    msg->type = type;
    memcpy(msg->text, text, copy_len);
    msg->text[copy_len] = '\0';

    // Update indices
    sg_write_index = (sg_write_index + 1) % CHAT_HISTORY_MAX_MESSAGES;
    if (sg_message_count < CHAT_HISTORY_MAX_MESSAGES) {
        sg_message_count++;
    }

    // Increment version on any change
    sg_version++;

    PR_DEBUG("Added %s message: %.*s", type == CHAT_MSG_TYPE_USER ? "USER" : "ASSISTANT", 
             (int)copy_len, text);
    
    return OPRT_OK;
}

OPERATE_RET app_chat_history_get_json(char *buffer, uint32_t buffer_size)
{
    if (!sg_initialized || buffer == NULL || buffer_size == 0) {
        return OPRT_INVALID_PARM;
    }

    // Start JSON array
    int written = snprintf(buffer, buffer_size, "{\"messages\":[");
    if (written < 0 || written >= (int)buffer_size) {
        return OPRT_COM_ERROR;
    }

    uint32_t offset = written;
    uint32_t start_idx = 0;

    // If buffer is full, start from oldest message
    if (sg_message_count == CHAT_HISTORY_MAX_MESSAGES) {
        start_idx = sg_write_index;
    }

    // Add messages
    for (uint32_t i = 0; i < sg_message_count; i++) {
        uint32_t idx = (start_idx + i) % CHAT_HISTORY_MAX_MESSAGES;
        CHAT_MESSAGE_T *msg = &sg_chat_history[idx];

        // Escape JSON special characters in text
        char escaped_text[CHAT_MESSAGE_MAX_LEN * 2] = {0};
        uint32_t escaped_len = 0;
        uint32_t text_len = strlen(msg->text);
        for (uint32_t j = 0; j < text_len && escaped_len < sizeof(escaped_text) - 1; j++) {
            char c = msg->text[j];
            if (c == '"') {
                if (escaped_len + 2 < sizeof(escaped_text)) {
                    escaped_text[escaped_len++] = '\\';
                    escaped_text[escaped_len++] = '"';
                }
            } else if (c == '\\') {
                if (escaped_len + 2 < sizeof(escaped_text)) {
                    escaped_text[escaped_len++] = '\\';
                    escaped_text[escaped_len++] = '\\';
                }
            } else if (c == '\n') {
                if (escaped_len + 2 < sizeof(escaped_text)) {
                    escaped_text[escaped_len++] = '\\';
                    escaped_text[escaped_len++] = 'n';
                }
            } else if (c == '\r') {
                if (escaped_len + 2 < sizeof(escaped_text)) {
                    escaped_text[escaped_len++] = '\\';
                    escaped_text[escaped_len++] = 'r';
                }
            } else if (c == '\t') {
                if (escaped_len + 2 < sizeof(escaped_text)) {
                    escaped_text[escaped_len++] = '\\';
                    escaped_text[escaped_len++] = 't';
                }
            } else if (c >= 0x20 || c == '\0') {
                // Only include printable characters or null terminator
                escaped_text[escaped_len++] = c;
            }
        }
        escaped_text[escaped_len] = '\0';

        // Add message JSON with image URL if present
        int msg_written;
        if (msg->has_image && msg->image_url[0] != '\0') {
            // Escape image URL
            char escaped_image_url[512 * 2] = {0};
            uint32_t escaped_img_len = 0;
            uint32_t img_url_len = strlen(msg->image_url);
            for (uint32_t j = 0; j < img_url_len && escaped_img_len < sizeof(escaped_image_url) - 1; j++) {
                char c = msg->image_url[j];
                if (c == '"') {
                    if (escaped_img_len + 2 < sizeof(escaped_image_url)) {
                        escaped_image_url[escaped_img_len++] = '\\';
                        escaped_image_url[escaped_img_len++] = '"';
                    }
                } else if (c == '\\') {
                    if (escaped_img_len + 2 < sizeof(escaped_image_url)) {
                        escaped_image_url[escaped_img_len++] = '\\';
                        escaped_image_url[escaped_img_len++] = '\\';
                    }
                } else {
                    escaped_image_url[escaped_img_len++] = c;
                }
            }
            escaped_image_url[escaped_img_len] = '\0';
            
            msg_written = snprintf(buffer + offset, buffer_size - offset,
                "%s{\"timestamp\":%u,\"type\":%d,\"text\":\"%s\",\"image_url\":\"%s\"}",
                (i > 0) ? "," : "",
                msg->timestamp,
                msg->type,
                escaped_text,
                escaped_image_url);
        } else {
            msg_written = snprintf(buffer + offset, buffer_size - offset,
                "%s{\"timestamp\":%u,\"type\":%d,\"text\":\"%s\"}",
                (i > 0) ? "," : "",
                msg->timestamp,
                msg->type,
                escaped_text);
        }

        if (msg_written < 0 || offset + msg_written >= buffer_size) {
            // Buffer too small, truncate
            break;
        }
        offset += msg_written;
    }

    // Close JSON array with version and streaming status
    int end_written = snprintf(buffer + offset, buffer_size - offset, 
                                "],\"count\":%u,\"version\":%u,\"is_streaming\":%s}", 
                                sg_message_count, sg_version, sg_is_streaming ? "true" : "false");
    if (end_written < 0 || offset + end_written >= buffer_size) {
        return OPRT_COM_ERROR;
    }

    return OPRT_OK;
}

uint32_t app_chat_history_get_count(void)
{
    return sg_message_count;
}

OPERATE_RET app_chat_history_clear(void)
{
    memset(sg_chat_history, 0, sizeof(sg_chat_history));
    sg_message_count = 0;
    sg_write_index = 0;
    sg_version++; // Increment version on clear
    PR_DEBUG("Chat history cleared");
    return OPRT_OK;
}

uint32_t app_chat_history_get_version(void)
{
    return sg_version;
}

OPERATE_RET app_chat_history_start_streaming_message(CHAT_MSG_TYPE_E type)
{
    if (!sg_initialized) {
        app_chat_history_init();
    }

    // Get current timestamp
    TIME_T posix_time = tal_time_get_posix();
    uint32_t timestamp = (uint32_t)posix_time;
    if (timestamp == 0) {
        timestamp = (uint32_t)time(NULL);
    }

    // Check if last message is the same type and can be updated
    // If not, create a new message
    bool can_update = false;
    if (sg_message_count > 0) {
        uint32_t last_idx = (sg_write_index == 0) ? 
            (CHAT_HISTORY_MAX_MESSAGES - 1) : (sg_write_index - 1);
        CHAT_MESSAGE_T *last_msg = &sg_chat_history[last_idx];
        // Only update if last message is same type and was recently created (within 10 seconds)
        if (last_msg->type == type && 
            (timestamp - last_msg->timestamp) < 10) {
            can_update = true;
            // Clear the message for streaming
            last_msg->text[0] = '\0';
            last_msg->timestamp = timestamp; // Update timestamp
        }
    }

    if (!can_update) {
        // Create new message entry
        CHAT_MESSAGE_T *msg = &sg_chat_history[sg_write_index];
        msg->timestamp = timestamp;
        msg->type = type;
        msg->text[0] = '\0'; // Start with empty text
        
        // Update indices
        sg_write_index = (sg_write_index + 1) % CHAT_HISTORY_MAX_MESSAGES;
        if (sg_message_count < CHAT_HISTORY_MAX_MESSAGES) {
            sg_message_count++;
        }
    }
    
    // Increment version when starting streaming message
    sg_version++;
    sg_is_streaming = true; // Mark as streaming

    PR_DEBUG("Started streaming message for type %d", type);
    return OPRT_OK;
}

void app_chat_history_end_streaming(void)
{
    sg_is_streaming = false;
    sg_version++; // Increment version to notify clients
    PR_DEBUG("Ended streaming message");
}

OPERATE_RET app_chat_history_append_to_last_message(const char *text, uint32_t len)
{
    if (!sg_initialized || text == NULL || len == 0) {
        return OPRT_INVALID_PARM;
    }

    if (sg_message_count == 0) {
        return OPRT_COM_ERROR; // No message to update
    }

    // Get last message index
    uint32_t last_idx = (sg_write_index == 0) ? 
        (CHAT_HISTORY_MAX_MESSAGES - 1) : (sg_write_index - 1);
    CHAT_MESSAGE_T *msg = &sg_chat_history[last_idx];

    // Calculate available space
    uint32_t current_len = strlen(msg->text);
    uint32_t available = CHAT_MESSAGE_MAX_LEN - current_len - 1;
    
    if (available == 0) {
        return OPRT_COM_ERROR; // Message is full
    }

    // Append text
    uint32_t copy_len = (len < available) ? len : available;
    memcpy(msg->text + current_len, text, copy_len);
    msg->text[current_len + copy_len] = '\0';

    // Increment version on text update
    sg_version++;

    PR_DEBUG("Appended %u bytes to last message (total: %u)", copy_len, current_len + copy_len);
    return OPRT_OK;
}

OPERATE_RET app_chat_history_add_image_to_last_message(const char *image_url)
{
    if (!sg_initialized || image_url == NULL || image_url[0] == '\0') {
        return OPRT_INVALID_PARM;
    }

    if (sg_message_count == 0) {
        return OPRT_COM_ERROR; // No message to update
    }

    // Get last message index
    uint32_t last_idx = (sg_write_index == 0) ? 
        (CHAT_HISTORY_MAX_MESSAGES - 1) : (sg_write_index - 1);
    CHAT_MESSAGE_T *msg = &sg_chat_history[last_idx];

    // Copy image URL
    uint32_t url_len = strlen(image_url);
    uint32_t copy_len = (url_len < sizeof(msg->image_url) - 1) ? url_len : sizeof(msg->image_url) - 1;
    memcpy(msg->image_url, image_url, copy_len);
    msg->image_url[copy_len] = '\0';
    msg->has_image = true;

    // Increment version on image update
    sg_version++;

    PR_DEBUG("Added image URL to last message: %s", image_url);
    return OPRT_OK;
}

OPERATE_RET app_chat_history_get_last_message_text(char *buffer, uint32_t buffer_size)
{
    if (!sg_initialized || buffer == NULL || buffer_size == 0) {
        return OPRT_INVALID_PARM;
    }

    if (sg_message_count == 0) {
        return OPRT_COM_ERROR; // No message
    }

    // Get last message index
    uint32_t last_idx = (sg_write_index == 0) ? 
        (CHAT_HISTORY_MAX_MESSAGES - 1) : (sg_write_index - 1);
    CHAT_MESSAGE_T *msg = &sg_chat_history[last_idx];

    uint32_t text_len = strlen(msg->text);
    uint32_t copy_len = (text_len < buffer_size - 1) ? text_len : buffer_size - 1;
    memcpy(buffer, msg->text, copy_len);
    buffer[copy_len] = '\0';

    return OPRT_OK;
}

OPERATE_RET app_chat_history_update_last_message_text(const char *text, uint32_t len)
{
    if (!sg_initialized || text == NULL) {
        return OPRT_INVALID_PARM;
    }

    if (sg_message_count == 0) {
        return OPRT_COM_ERROR; // No message to update
    }

    // Get last message index
    uint32_t last_idx = (sg_write_index == 0) ? 
        (CHAT_HISTORY_MAX_MESSAGES - 1) : (sg_write_index - 1);
    CHAT_MESSAGE_T *msg = &sg_chat_history[last_idx];

    uint32_t copy_len = len;
    if (copy_len >= CHAT_MESSAGE_MAX_LEN) {
        copy_len = CHAT_MESSAGE_MAX_LEN - 1;
    }

    memcpy(msg->text, text, copy_len);
    msg->text[copy_len] = '\0';

    // Increment version on text update
    sg_version++;

    return OPRT_OK;
}


