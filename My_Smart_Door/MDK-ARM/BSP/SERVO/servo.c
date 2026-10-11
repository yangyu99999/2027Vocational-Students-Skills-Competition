#include "servo.h"
static HAL_StatusTypeDef s_status=HAL_OK;
/* 功能：普通SG90位置控制；参数：TIM4/CH3及0..180度；返回：HAL状态。
 * 条件：72MHz定时器时钟，PSC=71、ARR=19999；首次上板先卸下舵盘验证。 */
HAL_StatusTypeDef Servo_SetAngle(TIM_HandleTypeDef *htim,uint32_t channel,uint16_t angle)
{
  uint32_t pulse;
  if(htim==NULL || htim->Instance!=TIM4 || channel!=TIM_CHANNEL_3 || angle>180U)
    return (s_status=HAL_ERROR);
  if(htim->Instance->PSC!=71U || htim->Instance->ARR!=19999U)
    return (s_status=HAL_ERROR);
  pulse=SERVO_MIN_PULSE_US+((SERVO_MAX_PULSE_US-SERVO_MIN_PULSE_US)*angle)/180U;
  __HAL_TIM_SET_COMPARE(htim,channel,pulse);
  /* 已启动时只更新CCR，重复Start会被本版HAL判为BUSY。 */
  if(HAL_TIM_GetChannelState(htim,channel)==HAL_TIM_CHANNEL_STATE_BUSY)
    s_status=HAL_OK;
  else
    s_status=HAL_TIM_PWM_Start(htim,channel);
  return s_status;
}
/* 功能：关闭PWM；返回：HAL状态；注意：关闭后舵机不保证保持位置。 */
HAL_StatusTypeDef Servo_Disable(TIM_HandleTypeDef *htim,uint32_t channel)
{
  if(htim==NULL || htim->Instance!=TIM4 || channel!=TIM_CHANNEL_3) return (s_status=HAL_ERROR);
  return (s_status=HAL_TIM_PWM_Stop(htim,channel));
}
/* 功能：读取最近控制状态；参数：无；返回：HAL状态。 */
HAL_StatusTypeDef Servo_GetStatus(void) { return s_status; }
/* 功能：兼容旧正转调用，定位180度；返回：无，状态由GetStatus读取。 */
void Servo_Forward(TIM_HandleTypeDef *htim,uint32_t channel) { (void)Servo_SetAngle(htim,channel,180U); }
/* 功能：兼容旧反转调用，定位0度；返回：无，状态由GetStatus读取。 */
void Servo_Reverse(TIM_HandleTypeDef *htim,uint32_t channel) { (void)Servo_SetAngle(htim,channel,0U); }
/* 功能：兼容旧停止调用，定位90度；返回：无，非关闭PWM。 */
void Servo_Stop(TIM_HandleTypeDef *htim,uint32_t channel) { (void)Servo_SetAngle(htim,channel,90U); }
