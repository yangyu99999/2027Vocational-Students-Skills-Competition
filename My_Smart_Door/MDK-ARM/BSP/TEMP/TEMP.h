#ifndef __TEMP_H
#define __TEMP_H

#include "adc.h"

/* ===== NTC 10K 热敏电阻温度采集（AO → PA1 / ADC1_IN1）=====
   接线：模块 VCC 必须接 3.3V（PA1 是模拟输入，不耐 5V），AO→PA1，GND→GND，DO 不接。
   分压 = 板上固定电阻 + NTC，AO 取中点：
     TEMP_NTC_TO_GND = 1 → NTC 接 GND 侧，温度升高 AO 电压下降
     TEMP_NTC_TO_GND = 0 → NTC 接 VCC 侧，温度升高 AO 电压上升
   验证方法：用手捏住 NTC，若温度显示反而变小，就把这个宏取反。
   ============================================================ */
#define TEMP_ADC_MAX        4095U
#define TEMP_VREF_MV        3300U      /* 模块与 MCU 同为 3.3V 供电 */
#define TEMP_FIXED_OHM      10000.0f   /* 与 NTC 串联的固定电阻，模块上一般为 10K */
#define TEMP_NTC_R0_OHM     10000.0f   /* NTC 在 25℃ 的阻值 */
#define TEMP_NTC_T0_K       298.15f    /* 25℃ 对应的开氏温度 */
#define TEMP_NTC_B          3950.0f    /* B 值，常见 10K B3950 */
#define TEMP_NTC_TO_GND     1U         /* 实测本模块为 NTC 在下臂：温度升高 AO 电压下降 */
#define TEMP_SAMPLE_COUNT   8U         /* 每次采样的平均次数 */
#define TEMP_REFRESH_MS     1000U      /* 采样节拍 */

#define TEMP_FORMAT_SIZE    8U         /* Temp_Format 输出缓冲区建议大小 */

/* 功能：初始化 ADC 通道（改长采样时间）、校准并先读一次；返回：1 成功 / 0 失败 */
uint8_t Temp_Init(void);

/* 功能：立刻采一次并按 B 值公式换算；参数：celsius_x10 单位 0.1℃；返回：1 正常 / 0 异常 */
uint8_t Temp_ReadX10(int16_t *celsius_x10);

/* 功能：按 1 秒节拍采样并缓存；返回：1 表示本次已采样（可用于刷新显示），0 表示未到节拍 */
uint8_t Temp_Update(void);

/* 功能：取最近一次采样结果；返回：0.1℃ 为单位，未初始化时为 0 */
int16_t Temp_GetX10(void);

/* 功能：查询最近一次采样是否成功；返回：1 有效 / 0 异常（未接、短路、超量程） */
uint8_t Temp_IsValid(void);

/* 功能：把最近一次结果格式化成文本；参数：out 至少 TEMP_FORMAT_SIZE 字节；返回：无 */
void Temp_Format(char *out);

#endif
