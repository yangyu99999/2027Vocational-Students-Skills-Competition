#ifndef __EEPROM_H
#define __EEPROM_H

#include "main.h"
#include "i2c.h"

/* AT24C16：2048 字节，16 字节/页，擦写寿命 100 万次。
 * 器件地址 1010 AAA：A2..A0 不是硬件引脚，而是字地址 bit10..8，
 * 所以一颗 24C16 独占 0xA0~0xAE 这 8 个地址，同一条总线上不能再挂第二颗。 */
#define EEPROM_I2C_BASE_ADDR   0xA0U   /* 1010 000 0，块选择位由字地址高 3 位拼进去 */
#define EEPROM_PAGE_SIZE       16U
#define EEPROM_TWR_MS          6U      /* 写周期 tWR 最大 5ms，留 1ms 余量 */
#define EEPROM_TIMEOUT_MS      10U

/* 功能：探测器件是否在线；参数：无；返回：1=在 0=不在 */
uint8_t EEPROM_IsPresent(void);
/* 功能：从 EEPROM 读一段数据（自动按页边界拆开，24C16 顺序读只在页内回卷）；参数：addr 字地址、buf、len；返回：1=成功 0=失败 */
uint8_t EEPROM_ReadBuffer(uint16_t addr, uint8_t *buf, uint16_t len);
/* 功能：往 EEPROM 写一段数据（自动按 16 字节页边界拆块，每块等 tWR）；参数：addr 字地址、buf、len；返回：1=成功 0=失败 */
uint8_t EEPROM_WriteBuffer(uint16_t addr, const uint8_t *buf, uint16_t len);

#endif
