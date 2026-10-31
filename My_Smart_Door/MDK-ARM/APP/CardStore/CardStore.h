#ifndef __CARD_STORE_H
#define __CARD_STORE_H

#include "main.h"
#include "../AppDef.h"

/* 卡片白名单的"持久化层"：上层只关心"能不能加/删/查"，
 * 不关心数据到底落在 AT24C16 还是片内 Flash。
 * 存储介质：开机探测 AT24C16（I2C1 PB6/PB7），没有则回落片内 Flash 最后一页。 */

/* 功能：开机初始化：探测介质、读回卡表；数据无效则装默认卡并写回；参数：无；返回：无 */
void CardStore_Init(void);

/* 功能：加一张卡并落盘；参数：uid；返回：0=成功 1=已在表里 2=表满 3=写入失败 */
uint8_t CardStore_Add(const uint8_t *uid);

/* 功能：删一张卡并落盘；参数：uid；返回：0=成功 1=不在表里 2=写入失败 */
uint8_t CardStore_Remove(const uint8_t *uid);

/* 功能：查卡是否在白名单；参数：uid；返回：1=在 0=不在 */
uint8_t CardStore_Contains(const uint8_t *uid);

/* 功能：当前已存卡数量；参数：无；返回：0..8 */
uint8_t CardStore_Count(void);

/* 功能：当前卡表在哪种介质上；参数：无；返回：1=AT24C16 0=片内 Flash */
uint8_t CardStore_OnEeprom(void);

#endif
