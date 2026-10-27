#include "TEMP.h"
#include <math.h>
#include <string.h>

static uint8_t s_temp_valid;   /* 最近一次采样是否有效 */
static int16_t s_temp_x10;     /* 最近一次温度，单位 0.1℃ */
static uint32_t s_temp_tick;   /* 上次采样时刻 */

/* 功能：采 TEMP_SAMPLE_COUNT 次取平均，按 B 值公式换算成 0.1℃；
   参数：celsius_x10 存放结果（单位 0.1℃）；返回：1 正常 / 0 传感器异常 */
uint8_t Temp_ReadX10(int16_t *celsius_x10)
{
  uint32_t sum = 0U;
  uint32_t raw;
  uint8_t i;
  float v_mv;
  float r_ntc;
  float inv_t;
  float celsius;
  int32_t t10;

  if (celsius_x10 == NULL)
  {
    return 0U;
  }

  for (i = 0U; i < TEMP_SAMPLE_COUNT; i++)
  {
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
      return 0U;
    }
    if (HAL_ADC_PollForConversion(&hadc1, 5U) != HAL_OK)
    {
      (void)HAL_ADC_Stop(&hadc1);
      return 0U;
    }
    sum += HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);
  }

  raw = sum / TEMP_SAMPLE_COUNT;
  if ((raw <= 8U) || (raw >= (TEMP_ADC_MAX - 8U)))
  {
    return 0U;                                   /* 0V 或满量程：没接模块/短路 */
  }

  v_mv = ((float)raw * (float)TEMP_VREF_MV) / (float)TEMP_ADC_MAX;

#if (TEMP_NTC_TO_GND != 0U)
  /* NTC 在下臂：V = VCC*Rt/(Rf+Rt) → Rt = Rf*V/(VCC-V) */
  if (v_mv >= ((float)TEMP_VREF_MV - 5.0f))
  {
    return 0U;
  }
  r_ntc = (TEMP_FIXED_OHM * v_mv) / ((float)TEMP_VREF_MV - v_mv);
#else
  /* NTC 在上臂：V = VCC*Rf/(Rt+Rf) → Rt = Rf*(VCC-V)/V */
  r_ntc = (TEMP_FIXED_OHM * ((float)TEMP_VREF_MV - v_mv)) / v_mv;
#endif

  /* B 值方程：1/T = 1/T0 + ln(Rt/R0)/B */
  inv_t = (1.0f / TEMP_NTC_T0_K) + (logf(r_ntc / TEMP_NTC_R0_OHM) / TEMP_NTC_B);
  if (inv_t <= 0.0f)
  {
    return 0U;
  }
  celsius = (1.0f / inv_t) - 273.15f;
  if ((celsius < -40.0f) || (celsius > 125.0f))
  {
    return 0U;                                   /* 超出量程：多半是分压方向配错 */
  }

  if (celsius >= 0.0f)
  {
    t10 = (int32_t)((celsius * 10.0f) + 0.5f);
  }
  else
  {
    t10 = (int32_t)((celsius * 10.0f) - 0.5f);
  }
  *celsius_x10 = (int16_t)t10;
  return 1U;
}

/* 功能：初始化：改长采样时间、校准、先读一次；返回：1 成功 / 0 失败 */
uint8_t Temp_Init(void)
{
  ADC_ChannelConfTypeDef cfg;
  int16_t value = 0;

  s_temp_valid = 0U;
  s_temp_x10 = 0;
  s_temp_tick = HAL_GetTick();

  /* 10K+10K 分压源阻抗约 5k，CubeMX 生成的 1.5 周期采样偏短，改为 55.5 周期。
     在代码里覆盖，CubeMX 重新生成工程也不会丢。 */
  cfg.Channel = ADC_CHANNEL_1;
  cfg.Rank = ADC_REGULAR_RANK_1;
  cfg.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &cfg) != HAL_OK)
  {
    return 0U;
  }

  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)   /* F1 必须先校准，否则读数整体偏移 */
  {
    return 0U;
  }

  if (Temp_ReadX10(&value) != 0U)
  {
    s_temp_x10 = value;
    s_temp_valid = 1U;
  }
  return s_temp_valid;
}

/* 功能：按 1 秒节拍采样并缓存；返回：1 本次已采样 / 0 未到节拍 */
uint8_t Temp_Update(void)
{
  uint32_t now = HAL_GetTick();
  int16_t value = 0;

  if ((uint32_t)(now - s_temp_tick) < TEMP_REFRESH_MS)
  {
    return 0U;
  }
  s_temp_tick = now;

  if (Temp_ReadX10(&value) != 0U)
  {
    s_temp_x10 = value;
    s_temp_valid = 1U;
  }
  else
  {
    s_temp_valid = 0U;      /* 保留上次数值但标记无效，显示端会改成 NO NTC */
  }
  return 1U;
}

/* 功能：取最近一次采样结果；返回：0.1℃ 为单位 */
int16_t Temp_GetX10(void)
{
  return s_temp_x10;
}

/* 功能：查询最近一次采样是否有效；返回：1 有效 / 0 异常 */
uint8_t Temp_IsValid(void)
{
  return s_temp_valid;
}

/* 功能：把最近一次结果格式化成 "25.3C"/"-5.0C"/"NO NTC"；参数：out 至少 TEMP_FORMAT_SIZE 字节 */
void Temp_Format(char *out)
{
  int32_t t10 = (int32_t)s_temp_x10;
  uint32_t whole;
  uint8_t pos = 0U;

  if (out == NULL)
  {
    return;
  }
  if (s_temp_valid == 0U)
  {
    (void)strcpy(out, "NO NTC");
    return;
  }

  if (t10 < 0)
  {
    out[pos++] = '-';
    t10 = -t10;
  }
  whole = (uint32_t)t10 / 10U;
  if (whole >= 100U)
  {
    out[pos++] = (char)('0' + (char)(whole / 100U));
  }
  if (whole >= 10U)
  {
    out[pos++] = (char)('0' + (char)((whole / 10U) % 10U));
  }
  out[pos++] = (char)('0' + (char)(whole % 10U));
  out[pos++] = '.';
  out[pos++] = (char)('0' + (char)((uint32_t)t10 % 10U));
  out[pos++] = 'C';
  out[pos] = '\0';
}
