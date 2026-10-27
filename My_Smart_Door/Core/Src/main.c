/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../../MDK-ARM/BSP/KEY/KEY.h"
#include "../../MDK-ARM/BSP/OLED/oled.h"
#include "../../MDK-ARM/BSP/SERVO/servo.h"
#include "../../MDK-ARM/BSP/BEEP/BEEP.h"
#include "../../MDK-ARM/BSP/FLASH/FLASH.h"
#include "../../MDK-ARM/BSP/RC522/RC522.h"
#include "../../MDK-ARM/BSP/TEMP/TEMP.h"
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  DOOR_LOCKED,
  DOOR_OPEN,
  PASSWORD_ERROR,
  CARD_DENIED,
  SERVO_ERROR
} DoorState;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/*定义*/
#define PASSWORD_LENGTH     6U
#define DOOR_CLOSE_ANGLE    180U
#define DOOR_OPEN_ANGLE     90U
#define DOOR_OPEN_TIME_MS   3000U
#define ERROR_BLINK_MS      200U
#define ERROR_BLINK_COUNT   3U
#define RFID_POLL_MS        150U
#define RFID_UID_LENGTH     4U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/*变量*/
static const char s_password[] = "123ABC";

static const char s_key_map[16] =
{
  '1', '2', '3', 'A',
  '4', '5', '6', 'B',
  '7', '8', '9', 'C',
  '*', '0', '#', 'D'
};
static char s_input[PASSWORD_LENGTH + 1U];
static uint8_t s_input_length;
static DoorState s_door_state = DOOR_LOCKED;
static uint32_t s_state_tick;
static uint8_t s_blink_on;
static uint8_t s_blink_count;
/* 白名单：允许开门的 4 字节 UID，字节顺序与 OLED 显示一致（从左到右）。
   加卡只需在表里加一行；本驱动只支持 4 字节 UID 的 M1 卡。 */
static const uint8_t s_rfid_whitelist[][RFID_UID_LENGTH] =
{
  { 0x79U, 0xE5U, 0xD0U, 0xE6U }   /* 卡 1：UID:79 E5 D0 E6 */
};
#define RFID_WHITELIST_COUNT (sizeof(s_rfid_whitelist) / sizeof(s_rfid_whitelist[0]))

static uint8_t s_rfid_ready;
static uint8_t s_rfid_uid_valid;
static uint8_t s_rfid_unsupported;
static uint8_t s_rfid_uid[RFID_UID_LENGTH];
static uint32_t s_rfid_poll_tick;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void ResetInput(void);
static void ShowPassword(void);
static void ShowInputPage(void);
static void SetServoError(void);
static void CheckPassword(void);
static void ProcessKeyInput(uint8_t key);
static void ProcessDoorState(void);
static void RFID_InitReader(void);
static void RFID_ShowStatus(void);
static void RFID_Process(void);
static void Temp_ShowStatus(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LED_GRE_GPIO_Port, LED_GRE_Pin, GPIO_PIN_SET);
  KEY_Init();
  OLED_Init();
  RFID_InitReader();
  Temp_Init();
  ResetInput();
  if (Servo_SetAngle(&htim4, TIM_CHANNEL_3, DOOR_CLOSE_ANGLE) == HAL_OK)
  {
    ShowInputPage();
  }
  else
  {
    SetServoError();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    ProcessDoorState();
    ProcessKeyInput(KEY_Scan());
    RFID_Process();
    if (Temp_Update() != 0U)
    {
      if (s_door_state == DOOR_LOCKED)
      {
        Temp_ShowStatus();   /* 只重画第一行，不清屏，不会闪 */
      }
    }
    HAL_Delay(2U);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/*重置输入*/
static void ResetInput(void)
{
  memset(s_input, 0, sizeof(s_input));
  s_input_length = 0U;
}

/*密码展示*/
static void ShowPassword(void)
{
  char line[PASSWORD_LENGTH + 1U];
  uint8_t i;
  for (i = 0U; i < PASSWORD_LENGTH; i++)
  {
    line[i] = (i < s_input_length) ? '*' : '_';
  }
  line[PASSWORD_LENGTH] = '\0';
  OLED_ShowString(0U, 2U, (uint8_t *)line, 16U);
}


static void ShowInputPage(void)
{
  OLED_Clear();
  Temp_ShowStatus();       /* 第一行左侧 PASSWORD:，右侧实时温度 */
  ShowPassword();
  OLED_ShowString(0U, 4U, (uint8_t *)"*:CLEAR #:OK", 16U);
  /* 16px 字体在 128x64 上只能放 4 行，最后一行改作读卡状态/卡号显示 */
  RFID_ShowStatus();
}

/*舵机错误*/
static void SetServoError(void)
{
  s_door_state = SERVO_ERROR;
  ResetInput();
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
    
  OLED_Clear();
  OLED_ShowString(0U, 0U, (uint8_t *)"SERVO ERROR", 16U);
  OLED_ShowString(0U, 2U, (uint8_t *)"CHECK AND RESET", 16U);
}

/* 功能：开门：舵机转开门角度、刷屏并启动自动关门；参数：title 第一行提示；返回：无 */
static void OpenDoor(const char *title)
{
  ResetInput();

  if (Servo_SetAngle(&htim4, TIM_CHANNEL_3, DOOR_OPEN_ANGLE) != HAL_OK)
  {
    SetServoError();
    return;
  }
  s_door_state = DOOR_OPEN;
  OLED_Clear();
  OLED_ShowString(0U, 0U, (uint8_t *)title, 16U);
  OLED_ShowString(0U, 2U, (uint8_t *)"DOOR OPEN", 16U);
  s_state_tick = HAL_GetTick();
}

/* 功能：进入拒绝状态：红灯闪 ERROR_BLINK_COUNT 次后回密码页；参数：state、两行提示；返回：无 */
static void EnterDenied(DoorState state, const char *line1, const char *line2)
{
  ResetInput();
  s_door_state = state;
  s_blink_count = 0U;
  s_blink_on = 1U;

  OLED_Clear();
  OLED_ShowString(0U, 0U, (uint8_t *)line1, 16U);
  if (line2 != NULL)
  {
    OLED_ShowString(0U, 2U, (uint8_t *)line2, 16U);
  }
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
  s_state_tick = HAL_GetTick();
}

/*密码检查*/
static void CheckPassword(void)
{
  uint8_t correct = (s_input_length == PASSWORD_LENGTH &&
                     memcmp(s_input, s_password, PASSWORD_LENGTH) == 0);

  ResetInput();
  if (correct != 0U)
  {
    OpenDoor("PASSWORD OK");
  }
  else
  {
    EnterDenied(PASSWORD_ERROR, "PASSWORD ERROR", NULL);
  }
}


/*按键输入*/
static void ProcessKeyInput(uint8_t key)
{
  char ch;
  if (s_door_state != DOOR_LOCKED || key == 0U || key > 16U)
  {
    return;
  }
  ch = s_key_map[key - 1U];
  if ((ch >= '0' && ch <= '9')||(ch >= 'A' && ch <= 'D'))
  {
    if (s_input_length < PASSWORD_LENGTH)
    {
      s_input[s_input_length++] = ch;
      s_input[s_input_length] = '\0';
      ShowPassword();
    }
  }
  else if (ch == '*')
  {
    ResetInput();
    ShowPassword();
  }
  else if (ch == '#')
  {
    CheckPassword();
  }
}

static void ProcessDoorState(void)
{
  uint32_t now = HAL_GetTick();   // 获取当前系统毫秒时间戳

  // ========== 分支1：拒绝状态（密码错误 PASSWORD_ERROR / 刷卡不在白名单 CARD_DENIED）==========
  if ((s_door_state == PASSWORD_ERROR || s_door_state == CARD_DENIED) &&
      (uint32_t)(now - s_state_tick) >= ERROR_BLINK_MS)
  {
    s_state_tick = now;                  // 更新本次闪烁触发时刻
    s_blink_on = (uint8_t)!s_blink_on;   // 翻转亮/灭标记
    // 控制红灯：s_blink_on 为真 → 拉低点亮（说明LED低电平点亮）
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin,
                      s_blink_on ? GPIO_PIN_RESET : GPIO_PIN_SET);

    if (!s_blink_on)  // 每次熄灭的时候计数一次（一次完整闪烁=亮+灭）
    {
      s_blink_count++;
      if (s_blink_count >= ERROR_BLINK_COUNT)
      {
        // 闪烁次数达标，恢复门锁状态，切回密码输入页面
        s_door_state = DOOR_LOCKED;
        ShowInputPage();
      }
    }
  }
  // ========== 分支2：门打开状态 DOOR_OPEN ==========
  else if (s_door_state == DOOR_OPEN &&
           (uint32_t)(now - s_state_tick) >= DOOR_OPEN_TIME_MS)
  {
    // 超时：舵机转到关门角度
    if (Servo_SetAngle(&htim4, TIM_CHANNEL_3, DOOR_CLOSE_ANGLE) != HAL_OK)
    {
      SetServoError();
      return;
    }
    s_door_state = DOOR_LOCKED;
    ShowInputPage();
  }
}

/* ================= RC522 刷卡：读 4 字节 UID，白名单内开门；不写卡、不改密钥 ================= */

/* 功能：把字符串补空格到 16 字符，覆盖整行避免残留旧字符；参数：dst 缓冲区、src 文本；返回：无 */
static void RFID_PutText(char *dst, const char *src)
{
  uint8_t i = 0U;

  while ((i < 16U) && (src[i] != '\0'))
  {
    dst[i] = src[i];
    i++;
  }
  for (; i < 16U; i++)
  {
    dst[i] = ' ';
  }
  dst[16] = '\0';
}

/* 功能：把最新读到的 4 字节 UID 格式化成 "UID:XX XX XX XX"（16 字符）；参数：line 至少 17 字节；返回：无 */
static void RFID_FormatUid(char *line)
{
  static const char hex[] = "0123456789ABCDEF";
  uint8_t i;

  line[0] = 'U';
  line[1] = 'I';
  line[2] = 'D';
  line[3] = ':';
  for (i = 0U; i < RFID_UID_LENGTH; i++)
  {
    line[4U + i * 3U] = hex[(s_rfid_uid[i] >> 4) & 0x0FU];
    line[5U + i * 3U] = hex[s_rfid_uid[i] & 0x0FU];
    line[6U + i * 3U] = (i < (RFID_UID_LENGTH - 1U)) ? ' ' : '\0';
  }
}

/* 功能：判断 4 字节 UID 是否在开门白名单；参数：uid；返回：1=允许 0=拒绝 */
static uint8_t RFID_IsAllowed(const uint8_t *uid)
{
  uint32_t card;
  uint8_t i;

  for (card = 0U; card < RFID_WHITELIST_COUNT; card++)
  {
    for (i = 0U; i < RFID_UID_LENGTH; i++)
    {
      if (uid[i] != s_rfid_whitelist[card][i])
      {
        break;
      }
    }
    if (i == RFID_UID_LENGTH)
    {
      return 1U;
    }
  }
  return 0U;
}

/* 功能：在 OLED 第 6 页显示读卡状态或 4 字节 UID；参数：无；返回：无 */
static void RFID_ShowStatus(void)
{
  char line[17];

  if (s_rfid_ready == 0U)
  {
    RFID_PutText(line, "RFID:NO MODULE");
  }
  else if (s_rfid_unsupported != 0U)
  {
    /* 选卡失败多为 7 字节 UID 卡，本驱动只做 4 字节 UID，不猜卡号 */
    RFID_PutText(line, "UID:SEL FAIL");
  }
  else if (s_rfid_uid_valid == 0U)
  {
    RFID_PutText(line, "UID:-- -- -- --");
  }
  else
  {
    RFID_FormatUid(line);
  }
  OLED_ShowString(0U, 6U, (uint8_t *)line, 16U);
}

/* 功能：复位 RC522、按 M1 卡参数配置并开天线，再读版本寄存器自检；参数：无；返回：无 */
static void RFID_InitReader(void)
{
  uint8_t version;

  s_rfid_ready = 0U;
  s_rfid_uid_valid = 0U;
  s_rfid_unsupported = 0U;
  s_rfid_poll_tick = HAL_GetTick();

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
  s_rfid_ready = 1U;
}

/* 功能：门锁定时周期寻卡，读到 UID 后按白名单决定开门或闪灯拒绝；参数：无；返回：无 */
static void RFID_Process(void)
{
  uint8_t tag_type[2];
  uint8_t uid[RFID_UID_LENGTH];
  uint32_t now = HAL_GetTick();
  uint8_t i;

  if (s_rfid_ready == 0U)
  {
    return;
  }
  if (s_door_state != DOOR_LOCKED)
  {
    return;                /* 开门/报错期间不读卡，避免拖慢关门与刷屏 */
  }
  if ((uint32_t)(now - s_rfid_poll_tick) < RFID_POLL_MS)
  {
    return;
  }
  s_rfid_poll_tick = now;

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
    s_rfid_unsupported = 1U;
    s_rfid_uid_valid = 0U;
    RFID_ShowStatus();
    return;
  }

  (void)PcdHalt();         /* 停卡：同一张卡贴住只算一次，移开后可再读 */

  for (i = 0U; i < RFID_UID_LENGTH; i++)
  {
    s_rfid_uid[i] = uid[i];
  }
  s_rfid_uid_valid = 1U;
  s_rfid_unsupported = 0U;

  if (RFID_IsAllowed(uid) != 0U)
  {
    OpenDoor("CARD OK");   /* 与密码开门共用同一入口，3 秒自动关门逻辑不变 */
  }
  else
  {
    char line[17];
    RFID_FormatUid(line);  /* 拒绝时把卡号显示出来，便于核对白名单 */
    EnterDenied(CARD_DENIED, "CARD DENIED", line);
  }
}

/* ============ 温度显示（采样与换算在 MDK-ARM/BSP/TEMP） ============ */

/* 功能：把温度画在密码页第一行右侧（与 "PASSWORD:" 同一行）；参数：无；返回：无 */
static void Temp_ShowStatus(void)
{
  char line[17];
  char val[TEMP_FORMAT_SIZE];
  uint8_t i;
  uint8_t len;
  uint8_t start;

  RFID_PutText(line, "PASSWORD:");
  Temp_Format(val);
  len = (uint8_t)strlen(val);
  start = (uint8_t)(16U - len);            /* 右对齐到最右一列 */
  if (start < 10U)
  {
    start = 10U;                           /* 不能压到 "PASSWORD:" 上 */
  }
  for (i = 0U; (i < len) && ((start + i) < 16U); i++)
  {
    line[start + i] = val[i];
  }
  OLED_ShowString(0U, 0U, (uint8_t *)line, 16U);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
