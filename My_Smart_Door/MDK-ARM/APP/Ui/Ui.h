#ifndef __UI_H
#define __UI_H

#include "main.h"
#include "../AppDef.h"

/* OLED 显示层：所有"往屏上写字"的代码都收在这里，
 * 别的模块只调函数，不直接调 OLED_xxx，也不自己拼 16 字符行。 */

/* 功能：初始化 OLED 并清屏；参数：无；返回：无 */
void Ui_Init(void);

/* 功能：整行 16 字符，右边用空格补满，避免残留旧字符；参数：dst 至少 17 字节、src；返回：无 */
void Ui_PutText(char *dst, const char *src);

/* 功能：把 4 字节 UID 格式化成 "UID:XX XX XX XX"；参数：uid、line（至少 17 字节）；返回：无 */
void Ui_FormatUid(const uint8_t *uid, char *line);

/* 功能：只重画第一行"PASSWORD:"+实时温度；参数：无；返回：无 */
void Ui_ShowTemp(void);

/* 功能：只重画密码掩码行；参数：input_len 已输入位数；返回：无 */
void Ui_ShowMask(uint8_t input_len);

/* 功能：只重画第 6 页读卡状态/卡号；参数：读卡视图；返回：无 */
void Ui_ShowCardLine(const RfidView *view);

/* 功能：完整密码页（温度行 + 掩码 + 按键提示 + 读卡状态）；参数：输入位数、读卡视图；返回：无 */
void Ui_ShowPasswordPage(uint8_t input_len, const RfidView *view);

/* 功能：清屏 + 最多三行提示（line2/line3 传 NULL 表示不显示）；参数：三行文本；返回：无 */
void Ui_ShowLines3(const char *line1, const char *line2, const char *line3);

/* 功能：开门页；参数：第一行标题；返回：无 */
void Ui_ShowDoorOpen(const char *title);

/* 功能：加/删卡等待刷卡页；参数：del_mode 0=加卡 1=删卡、已存张数；返回：无 */
void Ui_ShowAdminWait(uint8_t del_mode, uint8_t count);

/* 功能：加/删卡结果页；参数：模式、卡号行、结果文本；返回：无 */
void Ui_ShowAdminResult(uint8_t del_mode, const char *uid_line, const char *msg);

/* 功能：舵机故障页；参数：无；返回：无 */
void Ui_ShowServoError(void);

#endif
