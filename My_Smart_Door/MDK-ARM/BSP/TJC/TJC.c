#include "TJC.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
static HAL_StatusTypeDef s_status=HAL_OK;
/* 功能：发送标准陶晶驰指令和3个FF；参数：指令；返回：内部状态。
 * 条件：USART3=9600，屏幕已上电；这里只检查串口发送，不代表屏幕执行成功。 */
static void TJC_Send(const char *command)
{
  uint8_t buf[96];
  size_t len;
  if(command==NULL) { s_status=HAL_ERROR; return; }
  len=strlen(command);
  if(len>sizeof(buf)-3U) { s_status=HAL_ERROR; return; }
  memcpy(buf,command,len);
  buf[len++]=0xFFU; buf[len++]=0xFFU; buf[len++]=0xFFU;
  s_status=HAL_UART_Transmit(&huart3,buf,(uint16_t)len,120U);
}
/* 功能：切至small.HMI的page0；参数/返回：无；条件：屏幕启动完成后调用。 */
void bsp_tjc_init(void) { bsp_tjc_set_page(TJC_PAGE_HOME); }
/* 功能：切页；参数：仅支持当前HMI的0页；返回：状态由get_status获取。 */
void bsp_tjc_set_page(uint8_t page_id)
{
  if(page_id!=0U) { s_status=HAL_ERROR; return; }
  TJC_Send("page 0");
}
/* 功能：在t0显示字符串；ctrl_id=0，兼容层不把数值ID猜作控件名。
 * 参数：最多64个ASCII字符；拒绝引号、反斜线和控制字符以防破坏命令。 */
void bsp_tjc_show_str(uint8_t ctrl_id,char *str)
{
  char command[80];
  size_t i;
  if(ctrl_id!=TJC_TEXT_CONTROL || str==NULL) { s_status=HAL_ERROR; return; }
  for(i=0U;str[i]!='\0';i++)
    if(i>=64U || str[i]<' ' || (uint8_t)str[i]>126U || str[i]=='"' || str[i]=='\\')
      { s_status=HAL_ERROR; return; }
  (void)snprintf(command,sizeof(command),"t0.txt=\"%s\"",str);
  TJC_Send(command);
}
/* 功能：清空t0；参数：逻辑控件号0；返回：无。 */
void bsp_tjc_clear_str(uint8_t ctrl_id) { bsp_tjc_show_str(ctrl_id,""); }
/* 功能：设置t0的RGB565文字颜色；参数：逻辑控件号0、颜色；返回：无。 */
void bsp_tjc_set_pco(uint8_t ctrl_id,uint16_t color)
{
  char command[32];
  if(ctrl_id!=TJC_TEXT_CONTROL) { s_status=HAL_ERROR; return; }
  (void)snprintf(command,sizeof(command),"t0.pco=%u",(unsigned int)color);
  TJC_Send(command);
}
/* 功能：最近一次参数检查/发送结果；返回：HAL状态；不等同于屏幕ACK。 */
HAL_StatusTypeDef bsp_tjc_get_status(void) { return s_status; }
