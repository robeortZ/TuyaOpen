#ifndef __APP_PWM_H__
#define __APP_PWM_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET app_pwm_init(void);
OPERATE_RET app_pwm_set_duty(uint32_t duty);
OPERATE_RET app_pwm_deinit(void);

#ifdef __cplusplus
}
#endif

#endif