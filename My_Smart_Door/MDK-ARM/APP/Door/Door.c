#include "Door.h"
#include "../Ui/Ui.h"
#include "../Rfid/Rfid.h"
#include "../CardStore/CardStore.h"
#include "../../BSP/SERVO/servo.h"
#include <string.h>

/* ===== 业务参数：改这里就能改行为，不用翻状态机代码 ===== */
#define DOOR_PASSWORD         "123456"
#define DOOR_CLOSE_ANGLE      180U      /* 舵机关门角度 */
#define DOOR_OPEN_ANGLE       90U       /* 舵机开门角度 */
#define DOOR_OPEN_TIME_MS     3000U     /* 开门后自动关门时间 */
#define DOOR_BLINK_MS         200U      /* 红灯/绿灯闪烁半周期 */
#define DOOR_BLINK_COUNT      3U        /* 闪烁次数（亮+灭算一次） */
#define DOOR_ADMIN_TIMEOUT_MS 10000U    /* 加/删卡模式无操作自动退出 */
#define DOOR_ADMIN_RESULT_MS  1200U     /* 加/删卡结果停留时间 */

/* 4x4 键盘行/列 → 字符（与 BSP/KEY 的行列扫描顺序一致） */
static const char s_key_map[16] =
{
  'C', '9', '5', '1',
  'D', '0', '6', '2',
  '*', 'A', '7', '3',
  '#', 'B', '8', '4'
};

static char s_input[APP_PASSWORD_LENGTH + 1U];
static uint8_t s_input_length;
static DoorState s_state = DOOR_LOCKED;
static uint32_t s_state_tick;
static uint8_t s_blink_on;
static uint8_t s_blink_count;
static uint8_t s_green_on;      /* 开门成功的绿灯闪烁 */
static uint8_t s_green_count;
static uint32_t s_green_tick;

/* ===== 小工具 ===== */

/* 功能：红/绿灯开关（低电平点亮）；参数：on 1=亮；返回：无 */
static void led_red(uint8_t on)
{
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, (on != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void led_green(uint8_t on)
{
  HAL_GPIO_WritePin(LED_GRE_GPIO_Port, LED_GRE_Pin, (on != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/* 功能：清空密码输入；参数：无；返回：无 */
static void input_reset(void)
{
  memset(s_input, 0, sizeof(s_input));
  s_input_length = 0U;
}

/* 功能：重画密码页；参数：无；返回：无 */
static void show_password_page(void)
{
  RfidView view;

  Rfid_GetView(&view);
  Ui_ShowPasswordPage(s_input_length, &view);
}

/* 功能：进入舵机故障态（现场唯一需要断电/复位处理的异常）；参数：无；返回：无 */
static void servo_error(void)
{
  s_state = SERVO_ERROR;
  input_reset();
  led_red(1U);
  led_green(0U);
  Ui_ShowServoError();
}

/* 功能：开门：舵机转开门角度、刷屏、启动自动关门和绿灯闪烁；参数：title 第一行标题；返回：无 */
static void open_door(const char *title)
{
  input_reset();

  if (Servo_SetAngle(&htim4, TIM_CHANNEL_3, DOOR_OPEN_ANGLE) != HAL_OK)
  {
    servo_error();
    return;
  }
  s_state = DOOR_OPEN;
  Ui_ShowDoorOpen(title);
  s_state_tick = HAL_GetTick();

  /* 开锁成功：绿灯按与"密码错误红灯"相同的频率闪 3 次 */
  s_green_on = 1U;
  s_green_count = 0U;
  s_green_tick = s_state_tick;
  led_green(1U);
}

/* 功能：进入拒绝态（红灯闪 3 次后回密码页）；参数：状态、两行提示；返回：无 */
static void enter_denied(DoorState state, const char *line1, const char *line2)
{
  input_reset();
  s_state = state;
  s_blink_count = 0U;
  s_blink_on = 1U;

  Ui_ShowLines3(line1, line2, NULL);
  led_red(1U);
  led_green(0U);
  s_state_tick = HAL_GetTick();
}

/* 功能：校验键盘密码，对了开门、错了闪红灯；参数：无；返回：无 */
static void check_password(void)
{
  uint8_t correct = ((s_input_length == APP_PASSWORD_LENGTH) &&
                     (memcmp(s_input, DOOR_PASSWORD, APP_PASSWORD_LENGTH) == 0)) ? 1U : 0U;

  input_reset();
  if (correct != 0U)
  {
    open_door("PASSWORD OK");
  }
  else
  {
    enter_denied(PASSWORD_ERROR, "PASSWORD ERROR", NULL);
  }
}

/* 功能：进加卡/删卡模式；参数：del_mode 0=加卡 1=删卡；返回：无 */
static void enter_admin(uint8_t del_mode)
{
  input_reset();
  led_red(0U);
  led_green(1U);                    /* 绿灯常亮=等着刷卡 */

  s_state = (del_mode != 0U) ? ADMIN_DEL : ADMIN_ADD;
  s_state_tick = HAL_GetTick();
  Rfid_ResetPoll();

  Ui_ShowAdminWait(del_mode, CardStore_Count());
}

/* 功能：加/删卡模式读到卡后的处理与结果提示；参数：uid；返回：无 */
static void admin_handle_card(const uint8_t *uid)
{
  char line[17];
  const char *msg;
  uint8_t del_mode = (s_state == ADMIN_DEL) ? 1U : 0U;
  uint8_t result;

  if (del_mode == 0U)
  {
    result = CardStore_Add(uid);
    if (result == 0U)
    {
      msg = "ADDED OK";
    }
    else if (result == 1U)
    {
      msg = "ALREADY IN";
    }
    else if (result == 2U)
    {
      msg = "TABLE FULL";
    }
    else
    {
      msg = "WRITE ERR";
    }
  }
  else
  {
    result = CardStore_Remove(uid);
    if (result == 0U)
    {
      msg = "DELETED";
    }
    else if (result == 1U)
    {
      msg = "NOT FOUND";
    }
    else
    {
      msg = "WRITE ERR";
    }
  }

  Ui_FormatUid(uid, line);
  Ui_ShowAdminResult(del_mode, line, msg);
  led_green(0U);

  s_state = ADMIN_DONE;
  s_state_tick = HAL_GetTick();
}

/* ===== 对外接口 ===== */

void Door_Init(void)
{
  s_state = DOOR_LOCKED;
  s_state_tick = HAL_GetTick();
  input_reset();

  if (Servo_SetAngle(&htim4, TIM_CHANNEL_3, DOOR_CLOSE_ANGLE) == HAL_OK)
  {
    show_password_page();
  }
  else
  {
    servo_error();
  }
}

void Door_KeyInput(uint8_t key)
{
  char ch;

  if ((key == 0U) || (key > 16U))
  {
    return;
  }
  ch = s_key_map[key - 1U];

  /* 加/删卡模式：只认 * 取消，别的键不响应 */
  if ((s_state == ADMIN_ADD) || (s_state == ADMIN_DEL))
  {
    if (ch == '*')
    {
      led_green(0U);
      s_state = DOOR_LOCKED;
      show_password_page();
    }
    return;
  }
  if (s_state != DOOR_LOCKED)
  {
    return;
  }

  /* C 键：输入为空时进"加卡"；输入非空时仍当密码字符（密码里有 A/B/C） */
  if ((ch == 'C') && (s_input_length == 0U))
  {
    enter_admin(0U);
    return;
  }
  /* * 键：输入为空时进"删卡"；输入非空时先清空，保持原来的清空手感 */
  if (ch == '*')
  {
    if (s_input_length == 0U)
    {
      enter_admin(1U);
    }
    else
    {
      input_reset();
      Ui_ShowMask(s_input_length);
    }
    return;
  }
  if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'D'))
  {
    if (s_input_length < APP_PASSWORD_LENGTH)
    {
      s_input[s_input_length] = ch;
      s_input_length++;
      s_input[s_input_length] = '\0';
      Ui_ShowMask(s_input_length);
    }
  }
  else if (ch == '#')
  {
    check_password();
  }
}

void Door_OnCard(const uint8_t *uid)
{
  char line[17];

  if (uid == NULL)
  {
    return;
  }
  if ((s_state == ADMIN_ADD) || (s_state == ADMIN_DEL))
  {
    admin_handle_card(uid);
    return;
  }
  if (s_state != DOOR_LOCKED)
  {
    return;
  }

  if (CardStore_Contains(uid) != 0U)
  {
    open_door("CARD OK");          /* 与密码开门共用同一入口，3 秒自动关门不变 */
  }
  else
  {
    Ui_FormatUid(uid, line);       /* 拒绝时把卡号显示出来，便于核对白名单 */
    enter_denied(CARD_DENIED, "CARD DENIED", line);
  }
}

void Door_Poll(void)
{
  uint32_t now = HAL_GetTick();

  /* 分支0：开门成功后的绿灯闪烁（与红灯同频：200ms 翻转、共 3 次） */
  if ((s_state == DOOR_OPEN) && (s_green_count < DOOR_BLINK_COUNT) &&
      ((uint32_t)(now - s_green_tick) >= DOOR_BLINK_MS))
  {
    s_green_tick = now;
    s_green_on = (uint8_t)((s_green_on != 0U) ? 0U : 1U);
    led_green(s_green_on);
    if (s_green_on == 0U)          /* 每次熄灭计一次，亮+灭=一次完整闪烁 */
    {
      s_green_count++;
    }
  }

  /* 分支1：拒绝态（密码错误 / 刷卡不在白名单）→ 红灯闪 3 次后回密码页 */
  if (((s_state == PASSWORD_ERROR) || (s_state == CARD_DENIED)) &&
      ((uint32_t)(now - s_state_tick) >= DOOR_BLINK_MS))
  {
    s_state_tick = now;
    s_blink_on = (uint8_t)((s_blink_on != 0U) ? 0U : 1U);
    led_red(s_blink_on);
    if (s_blink_on == 0U)
    {
      s_blink_count++;
      if (s_blink_count >= DOOR_BLINK_COUNT)
      {
        s_state = DOOR_LOCKED;
        show_password_page();
      }
    }
  }
  /* 分支2：开门超时 → 舵机关门，回密码页 */
  else if ((s_state == DOOR_OPEN) && ((uint32_t)(now - s_state_tick) >= DOOR_OPEN_TIME_MS))
  {
    if (Servo_SetAngle(&htim4, TIM_CHANNEL_3, DOOR_CLOSE_ANGLE) != HAL_OK)
    {
      servo_error();
      return;
    }
    s_state = DOOR_LOCKED;
    led_green(0U);
    show_password_page();
  }
  /* 分支3：加/删卡等待刷卡超时 → 自动退出 */
  else if (((s_state == ADMIN_ADD) || (s_state == ADMIN_DEL)) &&
           ((uint32_t)(now - s_state_tick) >= DOOR_ADMIN_TIMEOUT_MS))
  {
    led_green(0U);
    s_state = DOOR_LOCKED;
    show_password_page();
  }
  /* 分支4：加/删卡结果停留结束 → 回密码页 */
  else if ((s_state == ADMIN_DONE) && ((uint32_t)(now - s_state_tick) >= DOOR_ADMIN_RESULT_MS))
  {
    s_state = DOOR_LOCKED;
    show_password_page();
  }
}

uint8_t Door_CardsReadable(void)
{
  if ((s_state == DOOR_LOCKED) || (s_state == ADMIN_ADD) || (s_state == ADMIN_DEL))
  {
    return 1U;
  }
  return 0U;
}

uint8_t Door_OnPasswordPage(void)
{
  return (s_state == DOOR_LOCKED) ? 1U : 0U;
}

DoorState Door_GetState(void)
{
  return s_state;
}
