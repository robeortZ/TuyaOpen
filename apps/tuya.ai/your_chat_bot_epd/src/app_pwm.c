#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_pwm.h"
#include "app_pwm.h"

#define PWM_CHANNEL TUYA_PWM_NUM_0
#define FREQUENCY 25000
#define MAX_DUTY 10000


OPERATE_RET app_pwm_init(void)
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_PWM_BASE_CFG_T pwm_cfg = {
        .duty = MAX_DUTY,
        .frequency = FREQUENCY, // 50Hz for servos
        .polarity = TUYA_PWM_NEGATIVE,
    };

    TUYA_CALL_ERR_RETURN(tkl_pwm_init(PWM_CHANNEL, &pwm_cfg));    
    TUYA_CALL_ERR_RETURN(tkl_pwm_start(PWM_CHANNEL));
    return rt;
}

OPERATE_RET app_pwm_set_duty( uint32_t duty)
{
    OPERATE_RET rt = OPRT_OK;

    if (duty > MAX_DUTY) {
        duty = MAX_DUTY;
    }
    if (duty < 0) {
        duty = 0;
    }
    TUYA_CALL_ERR_RETURN(tkl_pwm_duty_set(PWM_CHANNEL, duty));
    TUYA_CALL_ERR_RETURN(tkl_pwm_start(PWM_CHANNEL));
    return rt;
}

OPERATE_RET app_pwm_deinit(void)
{
    // tkl_pwm_deinit();
    //to do
    return OPRT_OK;
}