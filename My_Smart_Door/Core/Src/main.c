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
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../../MDK-ARM/BSP/KEY/KEY.h"
#include "../../MDK-ARM/BSP/OLED/oled.h"
#include "../../MDK-ARM/BSP/SERVO/servo.h"
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  DOOR_LOCKED,
  DOOR_OPEN,
  PASSWORD_ERROR,
  SERVO_ERROR
} DoorState;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PASSWORD_LENGTH     6U
#define DOOR_CLOSE_ANGLE    180U
#define DOOR_OPEN_ANGLE     90U
#define DOOR_OPEN_TIME_MS   3000U
#define ERROR_BLINK_MS      200U
#define ERROR_BLINK_COUNT   3U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static const char s_password[] = "123456";
/* �����ȼ�ֵ1..16��Ӧ�ַ������밴ʵ�ʼ�ñ�ͽ��ߵ����� */
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
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  /* ������HAL��GPIO��TIM4�ѳ�ʼ�����ϵ綨λ���ŽǶȡ� */
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LED_GRE_GPIO_Port, LED_GRE_Pin, GPIO_PIN_SET);
  KEY_Init();
  OLED_Init();
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
    /* ÿ��ɨ�裬��ʹ��ʾ����/����Ҳ�����¼�����ֹ�����Ӻ���Ч�� */
    ProcessKeyInput(KEY_Scan());
    ProcessDoorState();
    HAL_Delay(2U);

    /* USER CODE END 3 */
  }
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
/* ���ܣ�������룻����/���أ��ޣ���������ѭ�����ʼ�����á� */
static void ResetInput(void)
{
  memset(s_input, 0, sizeof(s_input));
  s_input_length = 0U;
}

/* ���ܣ���ˢ�������У�����/���أ��ޣ�������OLED�ѳ�ʼ���� */
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

/* ���ܣ����ƴ�����ҳ�棻����/���أ��ޣ�������OLED�ѳ�ʼ���� */
static void ShowInputPage(void)
{
  OLED_Clear();
  OLED_ShowString(0U, 0U, (uint8_t *)"PASSWORD:", 16U);
  ShowPassword();
  OLED_ShowString(0U, 4U, (uint8_t *)"*:CLEAR #:OK", 16U);
  OLED_ShowString(0U, 6U, (uint8_t *)"DOOR LOCKED", 16U);
}

/* ���ܣ���¼������ƴ��󣻲���/���أ��ޣ�������HAL���Ʒ���ʧ�ܡ�
 * ������󲻵�ͬ�ڻ�е������״̬�������Զ����ԣ����鲢��λ�� */
static void SetServoError(void)
{
  s_door_state = SERVO_ERROR;
  ResetInput();
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
  OLED_Clear();
  OLED_ShowString(0U, 0U, (uint8_t *)"SERVO ERROR", 16U);
  OLED_ShowString(0U, 2U, (uint8_t *)"CHECK AND RESET", 16U);
}

/* ���ܣ�ȷ�����벢�ı�״̬������/���أ��ޣ�����������״̬�°�#�� */
static void CheckPassword(void)
{
  uint8_t correct = (s_input_length == PASSWORD_LENGTH &&
                     memcmp(s_input, s_password, PASSWORD_LENGTH) == 0);
  ResetInput();
  if (correct)
  {
    /* ֻ��״̬�仯ʱ����һ�νǶȣ�����ѭ���з��������š� */
    if (Servo_SetAngle(&htim4, TIM_CHANNEL_3, DOOR_OPEN_ANGLE) != HAL_OK)
    {
      SetServoError();
      return;
    }
    s_door_state = DOOR_OPEN;
    OLED_Clear();
    OLED_ShowString(0U, 0U, (uint8_t *)"PASSWORD OK", 16U);
    OLED_ShowString(0U, 2U, (uint8_t *)"DOOR OPEN", 16U);
    s_state_tick = HAL_GetTick();
  }
  else
  {
    s_door_state = PASSWORD_ERROR;
    s_blink_count = 0U;
    s_blink_on = 1U;
    OLED_Clear();
    OLED_ShowString(0U, 0U, (uint8_t *)"PASSWORD ERROR", 16U);
    /* PB0�͵�ƽ��������������֮��ÿ200ms�л������������������ڡ� */
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
    s_state_tick = HAL_GetTick();
  }
}

/* ���ܣ�����һ�����������¼�����������ֵ0..16�����أ��ޡ�
 * ������ÿ����ѭ�����ã�������״̬�������룻��6λ������#ȷ�ϡ� */
static void ProcessKeyInput(uint8_t key)
{
  char ch;
  if (s_door_state != DOOR_LOCKED || key == 0U || key > 16U)
  {
    return;
  }
  ch = s_key_map[key - 1U];
  if (ch >= '0' && ch <= '9')
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

/* ���ܣ�ִ�����ƺ��Զ����ż�ʱ������/���أ��ޣ�������ÿ����ѭ�����á�
 * ʹ���޷���ʱ������HAL����������ƣ���ʹ�ó���ʱ��������ɨ�衣 */
static void ProcessDoorState(void)
{
  uint32_t now = HAL_GetTick();
  if (s_door_state == PASSWORD_ERROR &&
      (uint32_t)(now - s_state_tick) >= ERROR_BLINK_MS)
  {
    s_state_tick = now;
    s_blink_on = (uint8_t)!s_blink_on;
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin,
                      s_blink_on ? GPIO_PIN_RESET : GPIO_PIN_SET);
    if (!s_blink_on)
    {
      s_blink_count++;
      if (s_blink_count >= ERROR_BLINK_COUNT)
      {
        s_door_state = DOOR_LOCKED;
        ShowInputPage();
      }
    }
  }
  else if (s_door_state == DOOR_OPEN &&
           (uint32_t)(now - s_state_tick) >= DOOR_OPEN_TIME_MS)
  {
    if (Servo_SetAngle(&htim4, TIM_CHANNEL_3, DOOR_CLOSE_ANGLE) != HAL_OK)
    {
      SetServoError();
      return;
    }
    s_door_state = DOOR_LOCKED;
    ShowInputPage();
  }
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
