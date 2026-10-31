#include "EEPROM.h"

/* 功能：拼出本次访问要用的器件地址（24C16 把字地址 bit10..8 放进 A2..A0）；参数：字地址、读标志；返回：8 位器件地址 */
static uint8_t EEPROM_DevAddr(uint16_t addr, uint8_t is_read)
{
  uint8_t dev = (uint8_t)(EEPROM_I2C_BASE_ADDR | (uint8_t)(((addr >> 8) & 0x07U) << 1));

  if (is_read != 0U)
  {
    dev = (uint8_t)(dev | 0x01U);
  }
  return dev;
}

/* 功能：探测 AT24C16 是否应答；参数：无；返回：1=在 0=不在 */
uint8_t EEPROM_IsPresent(void)
{
  if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_DevAddr(0U, 0U), 3U, EEPROM_TIMEOUT_MS) == HAL_OK)
  {
    return 1U;
  }
  return 0U;
}

/* 功能：按页边界读一段；参数：addr、buf、len；返回：1=成功 0=失败 */
uint8_t EEPROM_ReadBuffer(uint16_t addr, uint8_t *buf, uint16_t len)
{
  uint16_t done = 0U;
  uint16_t chunk;
  uint16_t pos;

  if (buf == NULL)
  {
    return 0U;
  }
  while (done < len)
  {
    pos = (uint16_t)(addr + done);
    chunk = (uint16_t)(EEPROM_PAGE_SIZE - (pos % EEPROM_PAGE_SIZE));
    if (chunk > (uint16_t)(len - done))
    {
      chunk = (uint16_t)(len - done);
    }
    if (HAL_I2C_Mem_Read(&hi2c1, EEPROM_DevAddr(pos, 1U), (uint16_t)(pos & 0xFFU),
                         I2C_MEMADD_SIZE_8BIT, &buf[done], chunk, EEPROM_TIMEOUT_MS) != HAL_OK)
    {
      return 0U;
    }
    done = (uint16_t)(done + chunk);
  }
  return 1U;
}

/* 功能：按 16 字节页边界分块写，每块写完等 tWR；参数：addr、buf、len；返回：1=成功 0=失败 */
uint8_t EEPROM_WriteBuffer(uint16_t addr, const uint8_t *buf, uint16_t len)
{
  uint16_t done = 0U;
  uint16_t chunk;
  uint16_t pos;

  if (buf == NULL)
  {
    return 0U;
  }
  while (done < len)
  {
    pos = (uint16_t)(addr + done);
    chunk = (uint16_t)(EEPROM_PAGE_SIZE - (pos % EEPROM_PAGE_SIZE));
    if (chunk > (uint16_t)(len - done))
    {
      chunk = (uint16_t)(len - done);
    }
    if (HAL_I2C_Mem_Write(&hi2c1, EEPROM_DevAddr(pos, 0U), (uint16_t)(pos & 0xFFU),
                          I2C_MEMADD_SIZE_8BIT, (uint8_t *)&buf[done], chunk, EEPROM_TIMEOUT_MS) != HAL_OK)
    {
      return 0U;
    }
    HAL_Delay(EEPROM_TWR_MS);   /* 等内部写周期结束，否则下一次访问会 NACK */
    done = (uint16_t)(done + chunk);
  }
  return 1U;
}
