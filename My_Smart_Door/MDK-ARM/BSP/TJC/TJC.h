#ifndef __BSP_TJC_H
#define __BSP_TJC_H
#include "stm32f1xx_hal.h"
/* small.HMI中已识别page0及t0；不假设不存在的管理员/解锁页面。 */
#define TJC_PAGE_HOME 0U
#define TJC_TEXT_CONTROL 0U
void bsp_tjc_init(void);
void bsp_tjc_set_page(uint8_t page_id);
void bsp_tjc_show_str(uint8_t ctrl_id,char *str);
void bsp_tjc_clear_str(uint8_t ctrl_id);
void bsp_tjc_set_pco(uint8_t ctrl_id,uint16_t color);
HAL_StatusTypeDef bsp_tjc_get_status(void);
#endif
