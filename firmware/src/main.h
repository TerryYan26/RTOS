/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Sensor data structure */
typedef struct {
  uint32_t timestamp;
  float accel_x, accel_y, accel_z;    // m/s²
  float gyro_x, gyro_y, gyro_z;       // rad/s
  float pressure;                      // hPa
  float temperature;                   // °C
  float humidity;                      // %RH
  uint8_t data_valid;
} SensorData_t;

/* Control command structure */
typedef struct {
  uint8_t cmd_type;
  float target_value;
  uint32_t timestamp;
} ControlCmd_t;

/* Telemetry data structure */
typedef struct {
  uint32_t sequence;
  uint32_t timestamp;
  SensorData_t sensor_data;
  uint8_t system_status;
  float cpu_usage;
  uint32_t free_heap;
} TelemetryData_t;

/* Exported constants --------------------------------------------------------*/

/* System configuration constants */
#define SYSTEM_TICK_FREQ_HZ         1000
#define SENSOR_SAMPLE_RATE_HZ       100
#define FUSION_UPDATE_RATE_HZ       50
#define TELEMETRY_RATE_HZ           10

/* Sensor address definitions */
#define LSM6DSL_I2C_ADDR           0x6A
#define LPS22HB_I2C_ADDR           0x5C
#define HTS221_I2C_ADDR            0x5F

/* Task priority definitions */
#define TASK_PRIORITY_SENSOR       (tskIDLE_PRIORITY + 4)
#define TASK_PRIORITY_FUSION       (tskIDLE_PRIORITY + 3)
#define TASK_PRIORITY_CONTROL      (tskIDLE_PRIORITY + 3)
#define TASK_PRIORITY_TELEMETRY    (tskIDLE_PRIORITY + 2)
#define TASK_PRIORITY_WATCHDOG     (tskIDLE_PRIORITY + 1)

/* Task stack size definitions */
#define TASK_STACK_SIZE_SENSOR     512
#define TASK_STACK_SIZE_FUSION     512
#define TASK_STACK_SIZE_CONTROL    256
#define TASK_STACK_SIZE_TELEMETRY  1024
#define TASK_STACK_SIZE_WATCHDOG   256

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* External variables --------------------------------------------------------*/
extern I2C_HandleTypeDef hi2c2;
extern UART_HandleTypeDef huart1;
extern RTC_HandleTypeDef hrtc;

extern QueueHandle_t xSensorDataQueue;
extern QueueHandle_t xControlQueue;
extern QueueHandle_t xTelemetryQueue;
extern SemaphoreHandle_t xI2CMutex;

extern volatile uint32_t ulSystemTicks;
extern volatile uint8_t ucSystemReady;

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */