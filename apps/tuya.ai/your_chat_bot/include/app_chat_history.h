/**
 * @file app_chat_history.h
 * @brief Chat history storage and management module
 * @version 0.1
 * @date 2025-12-05
 */

#ifndef APP_CHAT_HISTORY_H
#define APP_CHAT_HISTORY_H

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
#define CHAT_HISTORY_MAX_MESSAGES 1000
#define CHAT_MESSAGE_MAX_LEN 2048
#define CHAT_HISTORY_JSON_BUFFER_SIZE (CHAT_HISTORY_MAX_MESSAGES * CHAT_MESSAGE_MAX_LEN)

/***********************************************************
***********************typedef define***********************
***********************************************************/
typedef enum {
    CHAT_MSG_TYPE_USER = 0,      // User message
    CHAT_MSG_TYPE_ASSISTANT = 1, // AI assistant message
} CHAT_MSG_TYPE_E;

typedef struct {
    uint32_t timestamp;          // Unix timestamp
    CHAT_MSG_TYPE_E type;        // Message type
    char text[CHAT_MESSAGE_MAX_LEN]; // Message text
    char image_url[512];         // Image URL (if message contains image)
    bool has_image;              // Whether message has image
} CHAT_MESSAGE_T;

/***********************************************************
***********************function define***********************
***********************************************************/

/**
 * @brief Initialize chat history module
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET app_chat_history_init(void);

/**
 * @brief Add a message to chat history
 * @param type Message type (USER or ASSISTANT)
 * @param text Message text
 * @param len Message text length
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET app_chat_history_add_message(CHAT_MSG_TYPE_E type, const char *text, uint32_t len);

/**
 * @brief Get chat history as JSON string
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET app_chat_history_get_json(char *buffer, uint32_t buffer_size);

/**
 * @brief Get number of messages in history
 * @return Number of messages
 */
uint32_t app_chat_history_get_count(void);

/**
 * @brief Clear all chat history
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET app_chat_history_clear(void);

/**
 * @brief Start a streaming message (for real-time updates)
 * @param type Message type (should be CHAT_MSG_TYPE_ASSISTANT for streaming)
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET app_chat_history_start_streaming_message(CHAT_MSG_TYPE_E type);

/**
 * @brief Update the last message with new text (for streaming)
 * @param text Additional text to append
 * @param len Text length
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET app_chat_history_append_to_last_message(const char *text, uint32_t len);

/**
 * @brief Add an image URL to the last message
 * @param image_url Image URL string
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET app_chat_history_add_image_to_last_message(const char *image_url);

/**
 * @brief Get the text content of the last message
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return OPERATE_RET - OPRT_OK on success, OPRT_COM_ERROR if no message
 */
OPERATE_RET app_chat_history_get_last_message_text(char *buffer, uint32_t buffer_size);

/**
 * @brief Update the text content of the last message
 * @param text New text content
 * @param len Text length
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET app_chat_history_update_last_message_text(const char *text, uint32_t len);

/**
 * @brief Get current version number (increments on any change)
 * @return Version number
 */
uint32_t app_chat_history_get_version(void);

/**
 * @brief End streaming message (removes cursor indicator)
 */
void app_chat_history_end_streaming(void);

#ifdef __cplusplus
}
#endif

#endif // APP_CHAT_HISTORY_H



