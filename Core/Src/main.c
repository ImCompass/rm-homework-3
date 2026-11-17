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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct __attribute__((packed))
{
  uint8_t header;     // 帧头 0xAA
  uint8_t run;        // 0=暂停 1=运行
  uint8_t dir;        // 0=正向 1=反向
  uint16_t step;      // 间隔 100~2000 ms，低字节在前
  uint8_t check;      // 前 5 字节之和的低 8 位
} CtrlPacket;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
CtrlPacket rx_packet;                 // 接收缓冲区，必须一直是全局变量
volatile uint8_t packet_ready = 0;    // 收满一包
volatile uint8_t dir_key_flag = 0;    // 方向键事件
volatile uint8_t pause_key_flag = 0;  // 暂停键事件

uint8_t run_state = 1;      // 1=运行，0=暂停
uint8_t direction = 0;      // 0=正向，1=反向
uint16_t step_ms = 300;     // 切换间隔
uint8_t led_index = 0;      // 当前点亮的 LED 编号 0~2
uint32_t last_step_tick = 0;// 上一次切灯的时间

volatile uint8_t tx_busy = 0;       // 串口中断发送是否正在进行
volatile uint8_t feedback_pending = 0; // 是否有反馈等待发送
uint8_t tx_buf[64];                 // 发送缓冲区，必须是全局变量
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void Update_LEDs(void);
void Update_OLED(void);
void Request_Feedback(void);
void Try_Send_Feedback(void);
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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  OLED_Init();   // 内部已经清屏，不用再调用 OLED_Clear()

  Update_LEDs();
  Update_OLED();

  last_step_tick = HAL_GetTick();
  HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_packet, (uint16_t)sizeof(rx_packet));
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // 1. 处理方向键
  if (dir_key_flag) {
    dir_key_flag = 0;
    direction = !direction;
    Update_OLED();
    Request_Feedback();
  }

  // 2. 处理暂停/继续键
  if (pause_key_flag) {
    pause_key_flag = 0;
    run_state = !run_state;
    last_step_tick = HAL_GetTick();
    Update_OLED();
    Request_Feedback();
  }

  // 3. 处理串口数据包
  if (packet_ready) {
    CtrlPacket packet = rx_packet;   // 先复制出来，再清标志
    packet_ready = 0;

    const uint8_t *bytes = (const uint8_t *)&packet;
    uint8_t check = (uint8_t)(bytes[0] + bytes[1] + bytes[2] + bytes[3] + bytes[4]);

    if (packet.header == 0xAA &&
      packet.run <= 1 &&
      packet.dir <= 1 &&
      packet.step >= 100 && packet.step <= 2000 &&
      packet.check == check) {      // 五项全过才执行，任何一项不对就丢掉这包

      run_state = packet.run;
      direction = packet.dir;
      step_ms = packet.step; 
      last_step_tick = HAL_GetTick();


      Update_OLED();
      Request_Feedback();
    }
    // 校验不通过就什么都不做，但接收必须重新启动

    HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_packet, (uint16_t)sizeof(rx_packet));
  }

  // 4. 非阻塞切灯：不使用 HAL_Delay
  if (run_state && (HAL_GetTick() - last_step_tick >= step_ms)) {
    last_step_tick = HAL_GetTick();

    if (direction == 0) {
      led_index = (led_index + 1) % 3;   // 正向：0→1→2→0
    } else {
      led_index = (led_index + 2) % 3;   // 反向：0→2→1→0
    }

    Update_LEDs();
    OLED_ShowNum(4, 6, led_index + 1, 1);   // 只刷新 LED 编号，整屏刷新太慢
  }

  // 5. 有反馈就尝试用串口中断发出去
  Try_Send_Feedback();
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED0_Pin|LED1_Pin|LED2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED0_Pin LED1_Pin LED2_Pin */
  GPIO_InitStruct.Pin = LED0_Pin|LED1_Pin|LED2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY_DIR_Pin */
  GPIO_InitStruct.Pin = KEY_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY_DIR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY_PAUSE_Pin */
  GPIO_InitStruct.Pin = KEY_PAUSE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY_PAUSE_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}



/* USER CODE BEGIN 4 */


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  static uint32_t last_dir_tick = 0;
  static uint32_t last_pause_tick = 0;
  uint32_t now = HAL_GetTick();

  if (GPIO_Pin == KEY_DIR_Pin) {
    // 20ms 内的重复下降沿当作抖动，不重复计数
    if (now - last_dir_tick >= 20) {
      last_dir_tick = now;
      dir_key_flag = 1;
    }
  }
  else if (GPIO_Pin == KEY_PAUSE_Pin) {
    if (now - last_pause_tick >= 20) {
      last_pause_tick = now;
      pause_key_flag = 1;
    }
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    packet_ready = 1;
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    tx_busy = 0;
  }
}

void Update_LEDs(void)
{
  // 先全灭，再点亮当前这一颗
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_RESET);

  if (led_index == 0) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
  } else if (led_index == 1) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
  } else {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
  }
}

void Update_OLED(void)
{
  char line[17];

  // 字符串末尾补空格，覆盖上一次显示留下的旧字符
  OLED_ShowString(1, 1, run_state ? "STATE: RUN    " : "STATE: PAUSE  ");
  OLED_ShowString(2, 1, direction ? "DIR: REV   " : "DIR: FWD   ");

  snprintf(line, sizeof(line), "STEP: %u ms   ", (unsigned int)step_ms);
  OLED_ShowString(3, 1, line);

  OLED_ShowString(4, 1, "LED: ");
  OLED_ShowNum(4, 6, led_index + 1, 1);
}

void Request_Feedback(void)
{
  feedback_pending = 1;
}

void Try_Send_Feedback(void)
{
  if (!feedback_pending || tx_busy) {
    return;
  }

  int len = snprintf((char *)tx_buf, sizeof(tx_buf),
           "STATE=%s,DIR=%s,STEP=%u,LED=%u\r\n",
           run_state ? "RUN" : "PAUSE",
           direction ? "REV" : "FWD",
           (unsigned int)step_ms,
           (unsigned int)(led_index + 1));

  if (len > 0 && HAL_UART_Transmit_IT(&huart1, tx_buf, (uint16_t)len) == HAL_OK) {
    tx_busy = 1;
    feedback_pending = 0;
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
