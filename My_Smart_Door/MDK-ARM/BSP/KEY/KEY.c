#include "KEY.h"
#define KEY_DEBOUNCE_MS 20U
static uint16_t s_candidate;
static uint16_t s_stable;
static uint32_t s_changed_at;

/* 功能：释放所有行并复位消抖；参数/返回：无；条件：GPIO初始化后调用。 */
void KEY_Init(void)
{
  HAL_GPIO_WritePin(GPIOA, ROW1_Pin|ROW2_Pin|ROW3_Pin|ROW4_Pin, GPIO_PIN_SET);
  s_candidate = 0U;
  s_stable = 0U;
  s_changed_at = HAL_GetTick();
}

/* 功能：扫描原始16位键值；返回：每一位对应一个键；条件：行开漏、列上拉。
 * 切换行后留出约数微秒建立时间，实际时间需逻辑分析仪确认。 */
uint16_t KEY_ScanMatrix(void)
{
  static const uint16_t rows[4] = {ROW1_Pin,ROW2_Pin,ROW3_Pin,ROW4_Pin};
  static const uint16_t cols[4] = {COL1_Pin,COL2_Pin,COL3_Pin,COL4_Pin};
  uint16_t mask = 0U;
  uint8_t r,c;
  volatile uint32_t settle;// volatile：防止编译器把空延时循环优化掉
  for (r=0U;r<4U;r++)
  {
    HAL_GPIO_WritePin(GPIOA, rows[r], GPIO_PIN_RESET);
    for(settle=0U;settle<128U;settle++) { __NOP(); }
    for(c=0U;c<4U;c++)
      if(HAL_GPIO_ReadPin(GPIOB,cols[c])==GPIO_PIN_RESET)
        mask |= (uint16_t)(1U << (r*4U+c));
    HAL_GPIO_WritePin(GPIOA, rows[r], GPIO_PIN_SET);
  }
  return mask;
}

/* 功能：返回一次稳定按下事件；返回：1..16，0无事件；每1..5ms调用。
 * 同时多键视为无效；必须稳定松开后才接受下一次按下，避免组合键误触发。 */
uint8_t KEY_Scan(void)
{
  uint16_t mask=KEY_ScanMatrix();
  uint32_t now=HAL_GetTick();
  uint8_t i;
  if(mask!=s_candidate) { s_candidate=mask; s_changed_at=now; }
  if((uint32_t)(now-s_changed_at)<KEY_DEBOUNCE_MS || mask==s_stable) return 0U;
  if(mask==0U) { s_stable=0U; return 0U; }
  if(s_stable!=0U) return 0U;
  s_stable=mask;
  if((mask & (uint16_t)(mask-1U))!=0U) return 0U;
  for(i=0U;i<16U;i++) if(mask & (1U<<i)) return (uint8_t)(i+1U);
  return 0U;
}
