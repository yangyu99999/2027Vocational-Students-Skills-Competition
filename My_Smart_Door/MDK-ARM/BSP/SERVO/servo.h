#ifndef __SERVO_H
#define __SERVO_H
#include "tim.h"
/* 普通SG90保守采用1..2ms；90度为1.5ms，实物角度和机械边界须确认。 */
#define SERVO_MIN_PULSE_US 500U
#define SERVO_MAX_PULSE_US 2500U
#define SERVO_STOP_PULSE_US 1500U
#define SERVO_FORWARD_PULSE_US 2500U
#define SERVO_REVERSE_PULSE_US 500U
HAL_StatusTypeDef Servo_SetAngle(TIM_HandleTypeDef *htim,uint32_t channel,uint16_t angle);
HAL_StatusTypeDef Servo_Disable(TIM_HandleTypeDef *htim,uint32_t channel);
HAL_StatusTypeDef Servo_GetStatus(void);
/* 兼容旧接口：Forward=180度，Reverse=0度，Stop=90度；并非连续旋转停止。 */
void Servo_Forward(TIM_HandleTypeDef *htim,uint32_t channel);
void Servo_Reverse(TIM_HandleTypeDef *htim,uint32_t channel);
void Servo_Stop(TIM_HandleTypeDef *htim,uint32_t channel);
#endif
