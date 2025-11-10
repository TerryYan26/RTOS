/**
 ******************************************************************************
 * @file           : sensor_acq.h
 * @brief          : Sensor data acquisition task header file
 * @author         : Your Name
 * @version        : V1.0.0
 * @date           : 2025-11-07
 ******************************************************************************
 */

#ifndef __SENSOR_ACQ_H
#define __SENSOR_ACQ_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "lsm6dsl.h"

/* Exported types ------------------------------------------------------------*/

/* Sensor acquisition states */
typedef enum {
  SENSOR_ACQ_INIT,
  SENSOR_ACQ_RUNNING,
  SENSOR_ACQ_ERROR,
  SENSOR_ACQ_STOPPED
} SensorAcqState_t;

/* Sensor acquisition statistics */
typedef struct {
  uint32_t total_samples;
  uint32_t error_count;
  uint32_t last_sample_time;
  float sample_rate;
  SensorAcqState_t state;
} SensorAcqStats_t;

/* Exported constants --------------------------------------------------------*/
#define SENSOR_SAMPLE_PERIOD_MS     (1000 / SENSOR_SAMPLE_RATE_HZ)

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief  Initialize sensor acquisition module
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef SensorAcq_Init(void);

/**
 * @brief  Create sensor acquisition task
 * @retval BaseType_t
 */
BaseType_t SensorAcq_CreateTask(void);

/**
 * @brief  Get sensor acquisition statistics
 * @param  stats: returned statistics
 * @retval None
 */
void SensorAcq_GetStats(SensorAcqStats_t *stats);

/**
 * @brief  Reset sensor acquisition statistics
 * @retval None
 */
void SensorAcq_ResetStats(void);

/**
 * @brief  Start/stop sensor acquisition
 * @param  enable: 1-start, 0-stop
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef SensorAcq_Enable(uint8_t enable);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_ACQ_H */