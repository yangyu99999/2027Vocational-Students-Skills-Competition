#ifndef __APP_DEF_H
#define __APP_DEF_H

#include <stdint.h>

/* 业务层共用定义。
 * 分层：main（启动 + 2ms 调度） → APP（业务） → BSP（寄存器/时序驱动）。
 * APP 内部再拆 CardStore（存储）/ Ui（显示）/ Rfid（读卡）/ Door（门禁逻辑），
 * 谁都不许直接动别人的全局变量，只走头文件里的接口。 */
#define APP_UID_LENGTH      4U   /* M1 卡 4 字节 UID */
#define APP_PASSWORD_LENGTH 6U   /* 键盘密码长度 */

typedef enum
{
  DOOR_LOCKED = 0,
  DOOR_OPEN,
  PASSWORD_ERROR,
  CARD_DENIED,
  ADMIN_ADD,
  ADMIN_DEL,
  ADMIN_DONE,
  SERVO_ERROR
} DoorState;

/* 读卡器对外的视图：UI 只认这个结构，不直接碰 RC522 的寄存器状态 */
typedef struct
{
  uint8_t ready;          /* 1=模块自检通过 */
  uint8_t select_failed;  /* 1=选卡失败（多为 7 字节 UID 卡，本驱动只做 4 字节） */
  uint8_t uid_valid;      /* 1=uid 字段里是最近读到的卡号 */
  uint8_t uid[APP_UID_LENGTH];
} RfidView;

#endif
