#ifndef __RFID_H
#define __RFID_H

#include "main.h"
#include "../AppDef.h"

/* 读卡层：负责 RC522 复位/配置/轮询，读到卡通过回调交出去。
 * 上层（Door）不用知道 PcdRequest/PcdAnticoll 这些细节。 */

/* 读到卡时的回调，参数是 4 字节 UID（生命周期到本次调用返回） */
typedef void (*RfidCardHandler)(const uint8_t *uid);

/* 功能：初始化读卡器（复位 + M1 参数 + 天线 + 版本自检）；参数：无；返回：无 */
void Rfid_Init(void);

/* 功能：周期寻卡；参数：allow 0=暂停寻卡 1=允许、refresh_line 1=选卡失败时刷新屏上卡号行；返回：无 */
void Rfid_Poll(uint8_t allow, uint8_t refresh_line);

/* 功能：注册读到卡的回调；参数：handler；返回：无 */
void Rfid_SetHandler(RfidCardHandler handler);

/* 功能：取最近一次读卡状态给 UI 用；参数：view 输出；返回：无 */
void Rfid_GetView(RfidView *view);

/* 功能：重置轮询节拍（进/出加删卡模式时调，避免刚进来就空等一个周期）；参数：无；返回：无 */
void Rfid_ResetPoll(void);

#endif
