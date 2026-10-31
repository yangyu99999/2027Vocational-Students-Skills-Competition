#include "Rfid.h"
#include "../Ui/Ui.h"
#include "../../BSP/RC522/RC522.h"
#include <string.h>

#define RFID_POLL_MS 150U   /* 寻卡周期：太快会打断主循环节奏，太慢刷卡要等 */

static RfidView s_view;
static uint32_t s_poll_tick;
static RfidCardHandler s_handler;

/* 功能：把当前视图交给回调并保留卡号；参数：uid；返回：无 */
static void rfid_dispatch(const uint8_t *uid)
{
  memcpy(s_view.uid, uid, APP_UID_LENGTH);
  s_view.uid_valid = 1U;
  s_view.select_failed = 0U;

  if (s_handler != NULL)
  {
    s_handler(s_view.uid);
  }
}

void Rfid_Init(void)
{
  uint8_t version;

  memset(&s_view, 0, sizeof(s_view));
  s_handler = NULL;
  s_poll_tick = HAL_GetTick();

  RC522_Init();            /* 驱动里只置 GPIO 电平，射频配置要自己补 */
  RC522_Rese();            /* 软复位，内部 50ms 上限，不会死等 */
  if (RC522_ResetTimedOut() != 0U)
  {
    return;                /* 复位超时：模块没接好或供电异常 */
  }

  RC522_Config_Type('A');  /* M1 卡参数 + 开天线 */

  version = RC522_Read_Register(VersionReg);
  /* 全 0/全 1 说明软 SPI 没通；0x91、0x92、0x12 都是可用的片 */
  if ((version == 0x00U) || (version == 0xFFU))
  {
    return;
  }
  s_view.ready = 1U;
}

void Rfid_SetHandler(RfidCardHandler handler)
{
  s_handler = handler;
}

void Rfid_ResetPoll(void)
{
  s_poll_tick = HAL_GetTick();
}

void Rfid_GetView(RfidView *view)
{
  if (view != NULL)
  {
    *view = s_view;
  }
}

void Rfid_Poll(uint8_t allow, uint8_t refresh_line)
{
  uint8_t tag_type[2];
  uint8_t uid[APP_UID_LENGTH];
  uint32_t now;

  if ((s_view.ready == 0U) || (allow == 0U))
  {
    return;
  }
  now = HAL_GetTick();
  if ((uint32_t)(now - s_poll_tick) < RFID_POLL_MS)
  {
    return;
  }
  s_poll_tick = now;

  if (PcdRequest(PICC_REQALL, tag_type) != MI_OK)
  {
    return;                /* 无卡：保留上次显示 */
  }
  if (PcdAnticoll(uid) != MI_OK)
  {
    return;                /* 防冲突失败，下一轮再试 */
  }
  if (PcdSelect(uid) != MI_OK)
  {
    s_view.select_failed = 1U;
    s_view.uid_valid = 0U;
    if (refresh_line != 0U)
    {
      Ui_ShowCardLine(&s_view);
    }
    return;
  }

  (void)PcdHalt();         /* 停卡：同一张卡贴住只算一次，移开后可再读 */
  rfid_dispatch(uid);
}
