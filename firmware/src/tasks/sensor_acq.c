/**
 ******************************************************************************
 * @file           : sensor_acq.c
 * @brief          : Sensor data acquisition task implementation
 * @author         : Your Name
 * @version        : V1.0.0
 * @date           : 2025-11-07
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "sensor_acq.h"
#include "lsm6dsl.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define SENSOR_ACQ_TASK_STACK_SIZE    TASK_STACK_SIZE_SENSOR
#define SENSOR_ACQ_TASK_PRIORITY      TASK_PRIORITY_SENSOR
#define SENSOR_ACQ_QUEUE_SIZE         10
#define SENSOR_MAX_RETRY_COUNT        3

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static TaskHandle_t xSensorAcqTaskHandle = NULL;
static SensorAcqStats_t xSensorStats = {0};
static uint8_t ucSensorEnabled = 0;
static LSM6DSL_Config_t xIMUConfig = {0};

/* Private function prototypes -----------------------------------------------*/
static void SensorAcqTask(void *pvParameters);
static HAL_StatusTypeDef SensorAcq_ReadIMU(SensorData_t *sensor_data);
static HAL_StatusTypeDef SensorAcq_ReadPressure(SensorData_t *sensor_data);
static HAL_StatusTypeDef SensorAcq_ReadHumidity(SensorData_t *sensor_data);
static void SensorAcq_UpdateStats(uint32_t sample_time);
static void SensorAcq_LogError(const char *error_msg);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  Initialize sensor acquisition module
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef SensorAcq_Init(void)
{
  HAL_StatusTypeDef status = HAL_OK;

  /* Initialize statistics */
  memset(&xSensorStats, 0, sizeof(SensorAcqStats_t));
  xSensorStats.state = SENSOR_ACQ_INIT;

  /* Configure LSM6DSL IMU */
  xIMUConfig.accel_odr = LSM6DSL_XL_ODR_104_HZ;
  xIMUConfig.accel_fs = LSM6DSL_XL_FS_2G;
  xIMUConfig.gyro_odr = LSM6DSL_GY_ODR_104_HZ;
  xIMUConfig.gyro_fs = LSM6DSL_GY_FS_250_DPS;
  xIMUConfig.fifo_enable = 0;

  /* Initialize LSM6DSL */
  status = LSM6DSL_Init(&xIMUConfig);
  if (status != HAL_OK) {
    SensorAcq_LogError("LSM6DSL initialization failed");
    xSensorStats.state = SENSOR_ACQ_ERROR;
    return status;
  }

  /* TODO: Initialize LPS22HB pressure sensor */
  /* TODO: Initialize HTS221 humidity sensor */

  xSensorStats.state = SENSOR_ACQ_INIT;
  return HAL_OK;
}

/**
 * @brief  创建传感器采集任务
 * @retval BaseType_t
 */
BaseType_t SensorAcq_CreateTask(void)
{
  BaseType_t result = pdPASS;

  /* 创建传感器采集任务 */
  result = xTaskCreate(SensorAcqTask, 
                       "SensorAcq", 
                       SENSOR_ACQ_TASK_STACK_SIZE, 
                       NULL, 
                       SENSOR_ACQ_TASK_PRIORITY, 
                       &xSensorAcqTaskHandle);

  if (result == pdPASS) {
    SensorAcq_LogError("SensorAcq task created successfully");
  } else {
    SensorAcq_LogError("Failed to create SensorAcq task");
  }

  return result;
}

/**
 * @brief  传感器采集任务主函数
 * @param  pvParameters: 任务参数
 * @retval None
 */
static void SensorAcqTask(void *pvParameters)
{
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(SENSOR_SAMPLE_PERIOD_MS);
  SensorData_t sensor_data = {0};
  HAL_StatusTypeDef status = HAL_OK;
  uint8_t retry_count = 0;

  /* 初始化周期性任务 */
  xLastWakeTime = xTaskGetTickCount();
  xSensorStats.state = SENSOR_ACQ_RUNNING;
  ucSensorEnabled = 1;

  SensorAcq_LogError("SensorAcq task started");

  /* 主任务循环 */
  for (;;) {
    if (ucSensorEnabled) {
      uint32_t sample_start_time = xTaskGetTickCount();
      
      /* 清除传感器数据 */
      memset(&sensor_data, 0, sizeof(SensorData_t));
      sensor_data.timestamp = sample_start_time;

      /* 读取IMU数据 */
      status = SensorAcq_ReadIMU(&sensor_data);
      if (status == HAL_OK) {
        /* 读取压力传感器数据 */
        status = SensorAcq_ReadPressure(&sensor_data);
      }
      
      if (status == HAL_OK) {
        /* 读取湿度传感器数据 */
        status = SensorAcq_ReadHumidity(&sensor_data);
      }

      if (status == HAL_OK) {
        sensor_data.data_valid = 1;
        retry_count = 0;

        /* 发送数据到融合任务 */
        if (xQueueSend(xSensorDataQueue, &sensor_data, pdMS_TO_TICKS(10)) != pdPASS) {
          SensorAcq_LogError("Failed to send sensor data to queue");
          xSensorStats.error_count++;
        }

        /* 更新统计信息 */
        SensorAcq_UpdateStats(sample_start_time);
      } else {
        /* 处理错误 */
        retry_count++;
        xSensorStats.error_count++;
        
        if (retry_count >= SENSOR_MAX_RETRY_COUNT) {
          SensorAcq_LogError("Max sensor read retries exceeded");
          xSensorStats.state = SENSOR_ACQ_ERROR;
          retry_count = 0;
          
          /* 尝试重新初始化传感器 */
          if (SensorAcq_Init() == HAL_OK) {
            xSensorStats.state = SENSOR_ACQ_RUNNING;
            SensorAcq_LogError("Sensor reinitialized successfully");
          }
        }
      }
    }

    /* 等待下一个采样周期 */
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

/**
 * @brief  读取IMU传感器数据
 * @param  sensor_data: 传感器数据结构
 * @retval HAL_StatusTypeDef
 */
static HAL_StatusTypeDef SensorAcq_ReadIMU(SensorData_t *sensor_data)
{
  HAL_StatusTypeDef status = HAL_OK;
  LSM6DSL_Data_t imu_data = {0};

  /* 读取LSM6DSL数据 */
  status = LSM6DSL_ReadData(&imu_data);
  if (status == HAL_OK && imu_data.data_ready) {
    /* 复制加速度数据 */
    sensor_data->accel_x = imu_data.accel_x;
    sensor_data->accel_y = imu_data.accel_y;
    sensor_data->accel_z = imu_data.accel_z;

    /* 复制陀螺仪数据 */
    sensor_data->gyro_x = imu_data.gyro_x;
    sensor_data->gyro_y = imu_data.gyro_y;
    sensor_data->gyro_z = imu_data.gyro_z;

    /* 复制温度数据（如果有效） */
    if (imu_data.temperature != 0.0f) {
      sensor_data->temperature = imu_data.temperature;
    }
  } else if (status == HAL_OK && !imu_data.data_ready) {
    /* 数据未准备就绪，不是错误 */
    status = HAL_OK;
  }

  return status;
}

/**
 * @brief  读取压力传感器数据
 * @param  sensor_data: 传感器数据结构
 * @retval HAL_StatusTypeDef
 */
static HAL_StatusTypeDef SensorAcq_ReadPressure(SensorData_t *sensor_data)
{
  /* TODO: 实现LPS22HB压力传感器读取 */
  /* 暂时使用模拟数据 */
  sensor_data->pressure = 1013.25f + (float)(rand() % 100 - 50) / 10.0f;
  return HAL_OK;
}

/**
 * @brief  读取湿度传感器数据
 * @param  sensor_data: 传感器数据结构
 * @retval HAL_StatusTypeDef
 */
static HAL_StatusTypeDef SensorAcq_ReadHumidity(SensorData_t *sensor_data)
{
  /* TODO: 实现HTS221湿度传感器读取 */
  /* 暂时使用模拟数据 */
  sensor_data->humidity = 45.0f + (float)(rand() % 200 - 100) / 10.0f;
  if (sensor_data->temperature == 0.0f) {
    sensor_data->temperature = 22.0f + (float)(rand() % 100 - 50) / 10.0f;
  }
  return HAL_OK;
}

/**
 * @brief  更新传感器采集统计信息
 * @param  sample_time: 采样时间戳
 * @retval None
 */
static void SensorAcq_UpdateStats(uint32_t sample_time)
{
  static uint32_t last_stats_update = 0;
  static uint32_t sample_count_in_period = 0;

  xSensorStats.total_samples++;
  xSensorStats.last_sample_time = sample_time;
  sample_count_in_period++;

  /* 每秒更新一次采样率统计 */
  if (sample_time - last_stats_update >= 1000) {
    xSensorStats.sample_rate = (float)sample_count_in_period * 1000.0f / 
                               (float)(sample_time - last_stats_update);
    last_stats_update = sample_time;
    sample_count_in_period = 0;
  }
}

/**
 * @brief  记录传感器错误日志
 * @param  error_msg: 错误消息
 * @retval None
 */
static void SensorAcq_LogError(const char *error_msg)
{
  char log_buffer[128];
  snprintf(log_buffer, sizeof(log_buffer), "[SensorAcq] %s (Tick: %lu)\r\n", 
           error_msg, xTaskGetTickCount());
  
  /* 通过UART输出日志 */
  HAL_UART_Transmit(&huart1, (uint8_t *)log_buffer, strlen(log_buffer), 100);
}

/**
 * @brief  获取传感器采集统计信息
 * @param  stats: 返回的统计信息
 * @retval None
 */
void SensorAcq_GetStats(SensorAcqStats_t *stats)
{
  if (stats != NULL) {
    taskENTER_CRITICAL();
    memcpy(stats, &xSensorStats, sizeof(SensorAcqStats_t));
    taskEXIT_CRITICAL();
  }
}

/**
 * @brief  重置传感器采集统计
 * @retval None
 */
void SensorAcq_ResetStats(void)
{
  taskENTER_CRITICAL();
  xSensorStats.total_samples = 0;
  xSensorStats.error_count = 0;
  xSensorStats.sample_rate = 0.0f;
  taskEXIT_CRITICAL();
}

/**
 * @brief  启动/停止传感器采集
 * @param  enable: 1-启动, 0-停止
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef SensorAcq_Enable(uint8_t enable)
{
  HAL_StatusTypeDef status = HAL_OK;

  if (enable && !ucSensorEnabled) {
    /* 启动传感器 */
    status = LSM6DSL_Enable(1);
    if (status == HAL_OK) {
      ucSensorEnabled = 1;
      xSensorStats.state = SENSOR_ACQ_RUNNING;
      SensorAcq_LogError("Sensor acquisition enabled");
    }
  } else if (!enable && ucSensorEnabled) {
    /* 停止传感器 */
    status = LSM6DSL_Enable(0);
    ucSensorEnabled = 0;
    xSensorStats.state = SENSOR_ACQ_STOPPED;
    SensorAcq_LogError("Sensor acquisition disabled");
  }

  return status;
}