/**
 * @file buddy_fsm.h
 * @brief Buddy state machine and pixel animation renderer.
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#ifndef __BUDDY_FSM_H__
#define __BUDDY_FSM_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----- State enum ----- */
typedef enum {
    BUDDY_STATE_DISCONNECTED = 0, /* No connection to bridge */
    BUDDY_STATE_IDLE,             /* Connected, no AI activity */
    BUDDY_STATE_BUSY,             /* Claude is processing */
    BUDDY_STATE_ATTENTION,        /* Waiting for user approval */
    BUDDY_STATE_CELEBRATE,        /* Session just completed */
    BUDDY_STATE_DIZZY,            /* Shake gesture triggered */
    BUDDY_STATE_MAX,
} buddy_state_e;

/* ----- Pending action (permission request) ----- */
#define BUDDY_ACTION_ID_LEN    32
#define BUDDY_PROMPT_LEN       64

typedef struct {
    char    id[BUDDY_ACTION_ID_LEN];     /* Action UUID */
    char    prompt[BUDDY_PROMPT_LEN];    /* Short description */
    bool    valid;                        /* Whether an action is pending */
} buddy_action_t;

/* ----- State info from bridge ----- */
typedef struct {
    buddy_state_e state;
    uint32_t      session_count;
    uint32_t      token_count;
    char          message[64];
    buddy_action_t action;
} buddy_state_info_t;

/* ----- API ----- */

/**
 * @brief Initialize the FSM and acquire the pixel handle.
 * @return OPRT_OK on success.
 */
OPERATE_RET buddy_fsm_init(void);

/**
 * @brief Apply a new state from the bridge.
 *        Resets animation tick on state change.
 */
void buddy_fsm_set_state(const buddy_state_info_t *info);

/**
 * @brief Get current state.
 */
buddy_state_e buddy_fsm_get_state(void);

/**
 * @brief Force a temporary DIZZY state (returns to previous after timeout).
 */
void buddy_fsm_trigger_dizzy(void);

/**
 * @brief Render one animation frame to the LED matrix.
 *        Call every BUDDY_RENDER_INTERVAL_MS ms from the main task.
 */
void buddy_fsm_render_tick(void);

/** Render interval in ms */
#define BUDDY_RENDER_INTERVAL_MS 50

#ifdef __cplusplus
}
#endif

#endif /* __BUDDY_FSM_H__ */
