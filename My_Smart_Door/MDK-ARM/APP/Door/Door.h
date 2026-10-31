#ifndef __DOOR_H
#define __DOOR_H

#include "main.h"
#include "../AppDef.h"

/* 门禁业务层：密码键盘、刷卡授权、加/删卡、状态机、舵机与指示灯。
 * 对外只暴露"初始化 / 周期处理 / 按键 / 读到卡"四个入口，main 不需要知道内部状态。 */

/* 功能：上电初始化（舵机关门 + 显示密码页）；参数：无；返回：无 */
void Door_Init(void);

/* 功能：状态机周期处理（绿灯闪烁、拒绝闪烁、3s 自动关门、加删卡超时）；参数：无；返回：无 */
void Door_Poll(void);

/* 功能：处理一个按键；参数：key 1..16，0=无键；返回：无 */
void Door_KeyInput(uint8_t key);

/* 功能：读到卡的处理（白名单开门 / 拒绝 / 加删卡）；参数：uid；返回：无 */
void Door_OnCard(const uint8_t *uid);

/* 功能：当前是否允许寻卡（门锁定或加删卡模式）；参数：无；返回：1=允许 0=暂停 */
uint8_t Door_CardsReadable(void);

/* 功能：当前是否停在密码页（决定读卡状态行要不要刷）；参数：无；返回：1=是 */
uint8_t Door_OnPasswordPage(void);

/* 功能：当前状态；参数：无；返回：DoorState */
DoorState Door_GetState(void);

#endif
