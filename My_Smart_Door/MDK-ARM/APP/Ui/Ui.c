#include "Ui.h"
#include "../../BSP/OLED/oled.h"
#include "../../BSP/TEMP/TEMP.h"
#include <string.h>

#define UI_LINE_CHARS 16U                                            /* 16px 字体一行 16 字符 */
#define UI_PAGE_0     0U                                             /* 128x64 只能放 4 行：0/2/4/6 页 */
#define UI_PAGE_1     2U
#define UI_PAGE_2     4U
#define UI_PAGE_3     6U

/* 功能：整行补空格后写一页；参数：page 页号、text；返回：无 */
static void ui_draw_line(uint8_t page, const char *text)
{
  char line[UI_LINE_CHARS + 1U];

  Ui_PutText(line, text);
  OLED_ShowString(0U, page, (uint8_t *)line, 16U);
}

void Ui_Init(void)
{
  OLED_Init();
  OLED_Clear();
}

void Ui_PutText(char *dst, const char *src)
{
  uint8_t i = 0U;

  while ((i < UI_LINE_CHARS) && (src[i] != '\0'))
  {
    dst[i] = src[i];
    i++;
  }
  for (; i < UI_LINE_CHARS; i++)
  {
    dst[i] = ' ';
  }
  dst[UI_LINE_CHARS] = '\0';
}

void Ui_FormatUid(const uint8_t *uid, char *line)
{
  static const char hex[] = "0123456789ABCDEF";
  uint8_t i;

  line[0] = 'U';
  line[1] = 'I';
  line[2] = 'D';
  line[3] = ':';
  for (i = 0U; i < APP_UID_LENGTH; i++)
  {
    line[4U + i * 3U] = hex[(uid[i] >> 4) & 0x0FU];
    line[5U + i * 3U] = hex[uid[i] & 0x0FU];
    line[6U + i * 3U] = (i < (APP_UID_LENGTH - 1U)) ? ' ' : '\0';
  }
}

void Ui_ShowTemp(void)
{
  char line[UI_LINE_CHARS + 1U];
  char val[TEMP_FORMAT_SIZE];
  uint8_t i;
  uint8_t len;
  uint8_t start;

  Ui_PutText(line, "PASSWORD:");
  Temp_Format(val);
  len = (uint8_t)strlen(val);
  start = (uint8_t)(UI_LINE_CHARS - len);   /* 右对齐到最右一列 */
  if (start < 10U)
  {
    start = 10U;                            /* 不能压到 "PASSWORD:" 上 */
  }
  for (i = 0U; (i < len) && ((uint8_t)(start + i) < UI_LINE_CHARS); i++)
  {
    line[start + i] = val[i];
  }
  line[UI_LINE_CHARS] = '\0';
  OLED_ShowString(0U, UI_PAGE_0, (uint8_t *)line, 16U);
}

void Ui_ShowMask(uint8_t input_len)
{
  char line[APP_PASSWORD_LENGTH + 1U];
  uint8_t i;

  for (i = 0U; i < APP_PASSWORD_LENGTH; i++)
  {
    line[i] = (i < input_len) ? '*' : '_';
  }
  line[APP_PASSWORD_LENGTH] = '\0';
  OLED_ShowString(0U, UI_PAGE_1, (uint8_t *)line, 16U);
}

void Ui_ShowCardLine(const RfidView *view)
{
  char line[UI_LINE_CHARS + 1U];

  if ((view == NULL) || (view->ready == 0U))
  {
    Ui_PutText(line, "RFID:NO MODULE");
  }
  else if (view->select_failed != 0U)
  {
    /* 选卡失败多为 7 字节 UID 卡，本驱动只做 4 字节，不猜卡号 */
    Ui_PutText(line, "UID:SEL FAIL");
  }
  else if (view->uid_valid == 0U)
  {
    Ui_PutText(line, "UID:-- -- -- --");
  }
  else
  {
    Ui_FormatUid(view->uid, line);
  }
  OLED_ShowString(0U, UI_PAGE_3, (uint8_t *)line, 16U);
}

void Ui_ShowPasswordPage(uint8_t input_len, const RfidView *view)
{
  OLED_Clear();
  Ui_ShowTemp();                      /* 第一行左侧 PASSWORD:，右侧实时温度 */
  Ui_ShowMask(input_len);
  ui_draw_line(UI_PAGE_2, "C:ADD *:DEL #:OK");
  Ui_ShowCardLine(view);              /* 最后一行用于读卡状态/卡号 */
}

void Ui_ShowLines3(const char *line1, const char *line2, const char *line3)
{
  OLED_Clear();
  if (line1 != NULL)
  {
    ui_draw_line(UI_PAGE_0, line1);
  }
  if (line2 != NULL)
  {
    ui_draw_line(UI_PAGE_1, line2);
  }
  if (line3 != NULL)
  {
    ui_draw_line(UI_PAGE_2, line3);
  }
}

void Ui_ShowDoorOpen(const char *title)
{
  OLED_Clear();
  ui_draw_line(UI_PAGE_0, title);
  ui_draw_line(UI_PAGE_1, "DOOR OPEN");
}

void Ui_ShowAdminWait(uint8_t del_mode, uint8_t count)
{
  char line[UI_LINE_CHARS + 1U];

  OLED_Clear();
  ui_draw_line(UI_PAGE_0, (del_mode != 0U) ? "DEL CARD" : "ADD CARD");
  ui_draw_line(UI_PAGE_1, "SWIPE CARD");
  Ui_PutText(line, "CARDS:");
  line[6] = (char)('0' + count);      /* 当前已存几张，方便现场核对 */
  OLED_ShowString(0U, UI_PAGE_2, (uint8_t *)line, 16U);
  ui_draw_line(UI_PAGE_3, "*:CANCEL");
}

void Ui_ShowAdminResult(uint8_t del_mode, const char *uid_line, const char *msg)
{
  OLED_Clear();
  ui_draw_line(UI_PAGE_0, (del_mode != 0U) ? "DEL CARD" : "ADD CARD");
  ui_draw_line(UI_PAGE_1, uid_line);
  ui_draw_line(UI_PAGE_2, msg);
  ui_draw_line(UI_PAGE_3, "DOOR LOCKED");
}

void Ui_ShowServoError(void)
{
  OLED_Clear();
  ui_draw_line(UI_PAGE_0, "SERVO ERROR");
  ui_draw_line(UI_PAGE_1, "CHECK AND RESET");
}
