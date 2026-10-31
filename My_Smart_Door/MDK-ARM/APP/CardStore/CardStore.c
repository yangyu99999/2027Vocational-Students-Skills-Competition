#include "CardStore.h"
#include "../../BSP/FLASH/FLASH.h"
#include "../../BSP/EEPROM/EEPROM.h"
#include <string.h>

/* 卡表在存储介质里的布局（11 个字 / 44 字节）：
 *   [0] 魔数 'CARD'  [1] 张数  [2..9] 8 张卡（每张 4 字节打包成一个字）  [10] 校验和 */
#define CARD_STORE_MAX        8U
#define CARD_STORE_MAGIC      0x43415244UL
#define CARD_STORE_WORDS      (2U + CARD_STORE_MAX + 1U)
#define CARD_STORE_EEPROM_ADDR 0x0000U      /* AT24C16 内偏移，44 字节只占前 3 页 */
#define CARD_STORE_FLASH_ADDR  0x0800F800UL /* 片内 Flash 页 62，程序只用到 ~21KB */

/* 默认白名单：首次上电（存储里没有有效数据）时装这一张 */
static const uint8_t s_default_cards[][APP_UID_LENGTH] =
{
  { 0x79U, 0xE5U, 0xD0U, 0xE6U }   /* UID:79 E5 D0 E6 */
};

static uint8_t s_cards[CARD_STORE_MAX][APP_UID_LENGTH];
static uint8_t s_count;
static uint8_t s_on_eeprom;

/* 功能：4 字节 UID 打包成一个字；参数：uid；返回：打包值 */
static uint32_t card_pack(const uint8_t *uid)
{
  return ((uint32_t)uid[0] << 24) | ((uint32_t)uid[1] << 16) |
         ((uint32_t)uid[2] << 8) | (uint32_t)uid[3];
}

/* 功能：把一个字拆回 4 字节 UID；参数：word、uid 输出；返回：无 */
static void card_unpack(uint32_t word, uint8_t *uid)
{
  uid[0] = (uint8_t)(word >> 24);
  uid[1] = (uint8_t)(word >> 16);
  uid[2] = (uint8_t)(word >> 8);
  uid[3] = (uint8_t)word;
}

/* 功能：卡表校验和（异或），判断存储里的数据有没有坏；参数：buf、字数；返回：校验值 */
static uint32_t card_checksum(const uint32_t *buf, uint8_t words)
{
  uint32_t sum = 0xA5A5A5A5UL;
  uint8_t i;

  for (i = 0U; i < words; i++)
  {
    sum ^= buf[i];
  }
  return sum;
}

/* 功能：判断读出来的卡表是否有效（魔数/张数/校验和）；参数：buf；返回：1=有效 0=无效 */
static uint8_t card_is_valid(const uint32_t *buf)
{
  uint8_t n = (uint8_t)(buf[1] & 0xFFU);

  if (buf[0] != CARD_STORE_MAGIC)
  {
    return 0U;
  }
  if (n > CARD_STORE_MAX)
  {
    return 0U;
  }
  if (card_checksum(buf, (uint8_t)(CARD_STORE_WORDS - 1U)) != buf[CARD_STORE_WORDS - 1U])
  {
    return 0U;
  }
  return 1U;
}

/* 功能：11 个字摊成 44 字节写进 EEPROM 并回读魔数确认；参数：buf；返回：1=成功 0=失败 */
static uint8_t card_write_eeprom(const uint32_t *buf)
{
  uint8_t bytes[CARD_STORE_WORDS * 4U];
  uint8_t check[4];
  uint32_t magic;
  uint8_t i;
  uint8_t j;

  for (i = 0U; i < CARD_STORE_WORDS; i++)
  {
    for (j = 0U; j < 4U; j++)
    {
      bytes[i * 4U + j] = (uint8_t)(buf[i] >> (8U * j));
    }
  }
  if (EEPROM_WriteBuffer(CARD_STORE_EEPROM_ADDR, bytes, (uint16_t)sizeof(bytes)) == 0U)
  {
    return 0U;
  }
  if (EEPROM_ReadBuffer(CARD_STORE_EEPROM_ADDR, check, 4U) == 0U)
  {
    return 0U;
  }
  magic = (uint32_t)check[0] | ((uint32_t)check[1] << 8) |
          ((uint32_t)check[2] << 16) | ((uint32_t)check[3] << 24);
  return (magic == CARD_STORE_MAGIC) ? 1U : 0U;
}

/* 功能：从 EEPROM 读回 44 字节拼成 11 个字；参数：buf（输出）；返回：1=成功 0=失败 */
static uint8_t card_read_eeprom(uint32_t *buf)
{
  uint8_t bytes[CARD_STORE_WORDS * 4U];
  uint32_t word;
  uint8_t i;
  uint8_t j;

  if (EEPROM_ReadBuffer(CARD_STORE_EEPROM_ADDR, bytes, (uint16_t)sizeof(bytes)) == 0U)
  {
    return 0U;
  }
  for (i = 0U; i < CARD_STORE_WORDS; i++)
  {
    word = 0U;
    for (j = 0U; j < 4U; j++)
    {
      word |= (uint32_t)bytes[i * 4U + j] << (8U * j);
    }
    buf[i] = word;
  }
  return 1U;
}

/* 功能：把 RAM 里的卡表落盘（EEPROM 优先，没有就擦写片内 Flash 最后一页）；参数：无；返回：1=成功 0=失败 */
static uint8_t card_save(void)
{
  uint32_t buf[CARD_STORE_WORDS];
  uint8_t i;

  buf[0] = CARD_STORE_MAGIC;
  buf[1] = (uint32_t)s_count;
  for (i = 0U; i < CARD_STORE_MAX; i++)
  {
    buf[2U + i] = (i < s_count) ? card_pack(s_cards[i]) : 0xFFFFFFFFUL;
  }
  buf[CARD_STORE_WORDS - 1U] = card_checksum(buf, (uint8_t)(CARD_STORE_WORDS - 1U));

  if (s_on_eeprom != 0U)
  {
    return card_write_eeprom(buf);
  }
  if (Flash_ErasePage(CARD_STORE_FLASH_ADDR) != FLASH_OK)
  {
    return 0U;
  }
  if (Flash_WriteArray(CARD_STORE_FLASH_ADDR, buf, CARD_STORE_WORDS) != FLASH_OK)
  {
    return 0U;
  }
  return (Flash_ReadWord(CARD_STORE_FLASH_ADDR) == CARD_STORE_MAGIC) ? 1U : 0U;
}

/* 功能：在卡表里找这张卡；参数：uid；返回：下标，找不到返回 0xFF */
static uint8_t card_find(const uint8_t *uid)
{
  uint8_t i;

  for (i = 0U; i < s_count; i++)
  {
    if (memcmp(s_cards[i], uid, APP_UID_LENGTH) == 0)
    {
      return i;
    }
  }
  return 0xFFU;
}

/* 功能：装默认卡并写回（存储里没有有效数据时用）；参数：无；返回：无 */
static void card_load_default(void)
{
  uint8_t i;

  s_count = (uint8_t)(sizeof(s_default_cards) / sizeof(s_default_cards[0]));
  for (i = 0U; i < s_count; i++)
  {
    memcpy(s_cards[i], s_default_cards[i], APP_UID_LENGTH);
  }
  (void)card_save();   /* 写不进去也不影响本次运行，只是下次开机还会重写 */
}

void CardStore_Init(void)
{
  uint32_t buf[CARD_STORE_WORDS];
  uint8_t i;
  uint8_t valid = 0U;

  s_count = 0U;
  s_on_eeprom = EEPROM_IsPresent();

  if (s_on_eeprom != 0U)
  {
    if (card_read_eeprom(buf) != 0U)
    {
      valid = card_is_valid(buf);
    }
  }
  else
  {
    for (i = 0U; i < CARD_STORE_WORDS; i++)
    {
      buf[i] = Flash_ReadWord(CARD_STORE_FLASH_ADDR + (uint32_t)i * 4U);
    }
    valid = card_is_valid(buf);
  }

  if (valid == 0U)
  {
    card_load_default();
    return;
  }

  s_count = (uint8_t)(buf[1] & 0xFFU);
  for (i = 0U; i < s_count; i++)
  {
    card_unpack(buf[2U + i], s_cards[i]);
  }
}

uint8_t CardStore_Add(const uint8_t *uid)
{
  if (uid == NULL)
  {
    return 3U;
  }
  if (card_find(uid) != 0xFFU)
  {
    return 1U;
  }
  if (s_count >= CARD_STORE_MAX)
  {
    return 2U;
  }
  memcpy(s_cards[s_count], uid, APP_UID_LENGTH);
  s_count++;
  if (card_save() == 0U)
  {
    s_count--;       /* 写失败就回滚：内存和存储都不认这张卡 */
    return 3U;
  }
  return 0U;
}

uint8_t CardStore_Remove(const uint8_t *uid)
{
  uint8_t index = card_find(uid);
  uint8_t i;

  if (index == 0xFFU)
  {
    return 1U;
  }
  /* 后面的卡往前挪一格，保证前 s_count 张连续 */
  for (i = index; (uint8_t)(i + 1U) < s_count; i++)
  {
    memcpy(s_cards[i], s_cards[i + 1U], APP_UID_LENGTH);
  }
  s_count--;
  if (card_save() == 0U)
  {
    return 2U;
  }
  return 0U;
}

uint8_t CardStore_Contains(const uint8_t *uid)
{
  if (uid == NULL)
  {
    return 0U;
  }
  return (card_find(uid) != 0xFFU) ? 1U : 0U;
}

uint8_t CardStore_Count(void)
{
  return s_count;
}

uint8_t CardStore_OnEeprom(void)
{
  return s_on_eeprom;
}
