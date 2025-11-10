/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : STM32L475E-IoT01A1 Real-Time Multi-Tasking Sensor Fusion System
 * @author         : Your Name
 * @version        : V1.0.0
 * @date           : 2025-11-07
 ******************************************************************************
 * @description    : FreeRTOS-based multi-sensor data acquisition, fusion and MQTT telemetry system
 *                  - Supports LSM6DSL IMU, LPS22HB pressure sensor, HTS221 humidity sensor
 *                  - Implements low-latency task scheduling (<50ms) and power optimization
 *                  - Provides MQTT telemetry and watchdog recovery mechanisms
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

/* Private includes ----------------------------------------------------------*/
#include "sensor_acq.h"
#include "fusion.h"
#include "control.h"
#include "telemetry.h"
#include "watchdog.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define MAIN_TASK_STACK_SIZE    (256)
#define MAIN_TASK_PRIORITY      (tskIDLE_PRIORITY + 1)

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Peripheral handles */
I2C_HandleTypeDef hi2c2;
UART_HandleTypeDef huart1;
RTC_HandleTypeDef hrtc;

/* FreeRTOS handles */
TaskHandle_t xMainTaskHandle = NULL;
QueueHandle_t xSensorDataQueue = NULL;
QueueHandle_t xControlQueue = NULL;
QueueHandle_t xTelemetryQueue = NULL;
SemaphoreHandle_t xI2CMutex = NULL;

/* System status */
volatile uint32_t ulSystemTicks = 0;
volatile uint8_t ucSystemReady = 0;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_RTC_Init(void);
void MainTask(void *pvParameters);
void Error_Handler(void);

/* Private user code ---------------------------------------------------------*/

/**
  * @brief  Application entry point
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();

  /* Create FreeRTOS objects */
  
  /* Create queues for inter-task communication */
  xSensorDataQueue = xQueueCreate(10, sizeof(SensorData_t));
  xControlQueue = xQueueCreate(5, sizeof(ControlCmd_t));
  xTelemetryQueue = xQueueCreate(20, sizeof(TelemetryData_t));
  
  /* Create mutex for I2C bus protection */
  xI2CMutex = xSemaphoreCreateMutex();
  
  if (xSensorDataQueue == NULL || xControlQueue == NULL || 
      xTelemetryQueue == NULL || xI2CMutex == NULL) {
    Error_Handler();
  }

  /* Create the main system task */
  if (xTaskCreate(MainTask, "MainTask", MAIN_TASK_STACK_SIZE, NULL, 
                  MAIN_TASK_PRIORITY, &xMainTaskHandle) != pdPASS) {
    Error_Handler();
  }

  /* Start the FreeRTOS scheduler */
  vTaskStartScheduler();

  /* We should never get here as control is now taken by the scheduler */
  while (1) {
    Error_Handler();
  }
}

/**
  * @brief  Main task - responsible for initializing other tasks and system monitoring
  * @param  pvParameters: task parameters
  * @retval None
  */
void MainTask(void *pvParameters)
{
  /* Initialize sensor drivers */
  if (SensorAcq_Init() != HAL_OK) {
    Error_Handler();
  }

  /* Create sensor acquisition task */
  if (SensorAcq_CreateTask() != pdPASS) {
    Error_Handler();
  }

  /* Create sensor fusion task */
  if (Fusion_CreateTask() != pdPASS) {
    Error_Handler();
  }

  /* Create control task */
  if (Control_CreateTask() != pdPASS) {
    Error_Handler();
  }

  /* Create telemetry task */
  if (Telemetry_CreateTask() != pdPASS) {
    Error_Handler();
  }

  /* Create watchdog task */
  if (Watchdog_CreateTask() != pdPASS) {
    Error_Handler();
  }

  /* System ready flag */
  ucSystemReady = 1;
  
  /* Main task loop - system status monitoring */
  for (;;) {
    /* Update system ticks */
    ulSystemTicks = xTaskGetTickCount();
    
    /* System status LED blink */
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14); // Green LED
    
    /* Periodic task - execute every 1 second */
    vTaskDelay(pdMS_TO_TICKS(1000));
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x10909CEC;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
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
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET); // LED2 (green)

  /*Configure GPIO pin : PtPin */
  GPIO_InitStruct.Pin = GPIO_PIN_13; // User button
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PtPin */
  GPIO_InitStruct.Pin = GPIO_PIN_14; // LED2
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 15, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
    /* Error indication - fast blinking red LED */
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
    HAL_Delay(100);
  }
}

/* FreeRTOS hooks */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  /* Stack overflow detected */
  Error_Handler();
}

void vApplicationMallocFailedHook(void)
{
  /* Memory allocation failed */
  Error_Handler();
}

void vApplicationIdleHook(void)
{
  /* Tickless idle for power optimization */
  __WFI();
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
}
#endif /* USE_FULL_ASSERT */