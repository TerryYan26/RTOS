/**
 ******************************************************************************
 * @file           : lsm6dsl.h
 * @brief          : LSM6DSL IMU sensor driver header file
 * @author         : Your Name
 * @version        : V1.0.0
 * @date           : 2025-11-07
 ******************************************************************************
 * @description    : LSM6DSL 6-axis inertial measurement unit driver
 *                  - 3-axis accelerometer (±2/±4/±8/±16 g)
 *                  - 3-axis gyroscope (±125/±250/±500/±1000/±2000 dps)
 *                  - I2C interface communication
 ******************************************************************************
 */

#ifndef __LSM6DSL_H
#define __LSM6DSL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported types ------------------------------------------------------------*/

/* LSM6DSL configuration structure */
typedef struct {
  uint8_t accel_odr;        // Accelerometer output data rate
  uint8_t accel_fs;         // Accelerometer full scale
  uint8_t gyro_odr;         // Gyroscope output data rate
  uint8_t gyro_fs;          // Gyroscope full scale
  uint8_t fifo_enable;      // FIFO enable
} LSM6DSL_Config_t;

/* LSM6DSL data structure */
typedef struct {
  float accel_x, accel_y, accel_z;    // Acceleration (m/s²)
  float gyro_x, gyro_y, gyro_z;       // Angular velocity (rad/s)
  float temperature;                  // Temperature (°C)
  uint32_t timestamp;                 // Timestamp
  uint8_t data_ready;                 // Data ready flag
} LSM6DSL_Data_t;

/* Exported constants --------------------------------------------------------*/

/* LSM6DSL device addresses */
#define LSM6DSL_I2C_ADDRESS_LOW     0x6A
#define LSM6DSL_I2C_ADDRESS_HIGH    0x6B

/* LSM6DSL register addresses */
#define LSM6DSL_WHO_AM_I            0x0F
#define LSM6DSL_CTRL1_XL            0x10
#define LSM6DSL_CTRL2_G             0x11
#define LSM6DSL_CTRL3_C             0x12
#define LSM6DSL_CTRL4_C             0x13
#define LSM6DSL_CTRL5_C             0x14
#define LSM6DSL_CTRL6_C             0x15
#define LSM6DSL_CTRL7_G             0x16
#define LSM6DSL_CTRL8_XL            0x17
#define LSM6DSL_CTRL9_XL            0x18
#define LSM6DSL_CTRL10_C            0x19

#define LSM6DSL_STATUS_REG          0x1E
#define LSM6DSL_OUT_TEMP_L          0x20
#define LSM6DSL_OUT_TEMP_H          0x21
#define LSM6DSL_OUTX_L_G            0x22
#define LSM6DSL_OUTX_H_G            0x23
#define LSM6DSL_OUTY_L_G            0x24
#define LSM6DSL_OUTY_H_G            0x25
#define LSM6DSL_OUTZ_L_G            0x26
#define LSM6DSL_OUTZ_H_G            0x27
#define LSM6DSL_OUTX_L_XL           0x28
#define LSM6DSL_OUTX_H_XL           0x29
#define LSM6DSL_OUTY_L_XL           0x2A
#define LSM6DSL_OUTY_H_XL           0x2B
#define LSM6DSL_OUTZ_L_XL           0x2C
#define LSM6DSL_OUTZ_H_XL           0x2D

/* WHO_AM_I value */
#define LSM6DSL_WHO_AM_I_VALUE      0x6A

/* Accelerometer output data rate configuration */
#define LSM6DSL_XL_ODR_POWER_DOWN   0x00
#define LSM6DSL_XL_ODR_12_5_HZ      0x10
#define LSM6DSL_XL_ODR_26_HZ        0x20
#define LSM6DSL_XL_ODR_52_HZ        0x30
#define LSM6DSL_XL_ODR_104_HZ       0x40
#define LSM6DSL_XL_ODR_208_HZ       0x50
#define LSM6DSL_XL_ODR_416_HZ       0x60
#define LSM6DSL_XL_ODR_833_HZ       0x70
#define LSM6DSL_XL_ODR_1_66_KHZ     0x80
#define LSM6DSL_XL_ODR_3_33_KHZ     0x90
#define LSM6DSL_XL_ODR_6_66_KHZ     0xA0

/* Accelerometer full scale configuration */
#define LSM6DSL_XL_FS_2G            0x00
#define LSM6DSL_XL_FS_16G           0x04
#define LSM6DSL_XL_FS_4G            0x08
#define LSM6DSL_XL_FS_8G            0x0C

/* Gyroscope output data rate configuration */
#define LSM6DSL_GY_ODR_POWER_DOWN   0x00
#define LSM6DSL_GY_ODR_12_5_HZ      0x10
#define LSM6DSL_GY_ODR_26_HZ        0x20
#define LSM6DSL_GY_ODR_52_HZ        0x30
#define LSM6DSL_GY_ODR_104_HZ       0x40
#define LSM6DSL_GY_ODR_208_HZ       0x50
#define LSM6DSL_GY_ODR_416_HZ       0x60
#define LSM6DSL_GY_ODR_833_HZ       0x70
#define LSM6DSL_GY_ODR_1_66_KHZ     0x80
#define LSM6DSL_GY_ODR_3_33_KHZ     0x90
#define LSM6DSL_GY_ODR_6_66_KHZ     0xA0

/* Gyroscope full scale configuration */
#define LSM6DSL_GY_FS_125_DPS       0x02
#define LSM6DSL_GY_FS_250_DPS       0x00
#define LSM6DSL_GY_FS_500_DPS       0x04
#define LSM6DSL_GY_FS_1000_DPS      0x08
#define LSM6DSL_GY_FS_2000_DPS      0x0C

/* Status register bit definitions */
#define LSM6DSL_STATUS_TDA          0x04  // Temperature data available
#define LSM6DSL_STATUS_GDA          0x02  // Gyroscope data available
#define LSM6DSL_STATUS_XLDA         0x01  // Accelerometer data available

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief  Initialize LSM6DSL sensor
 * @param  config: sensor configuration parameters
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_Init(LSM6DSL_Config_t *config);

/**
 * @brief  Read LSM6DSL WHO_AM_I register
 * @param  who_am_i: returned device ID
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_ReadWhoAmI(uint8_t *who_am_i);

/**
 * @brief  Read LSM6DSL sensor data
 * @param  data: returned sensor data
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_ReadData(LSM6DSL_Data_t *data);

/**
 * @brief  检查数据是否准备就绪
 * @param  status: 返回的状态寄存器值
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_GetStatus(uint8_t *status);

/**
 * @brief  软件复位LSM6DSL
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_SoftReset(void);

/**
 * @brief  使能/禁用LSM6DSL
 * @param  enable: 1-使能, 0-禁用
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_Enable(uint8_t enable);

/**
 * @brief  配置LSM6DSL中断
 * @param  int_config: 中断配置
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_ConfigInterrupt(uint8_t int_config);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  LSM6DSL寄存器读取
 * @param  reg_addr: 寄存器地址
 * @param  data: 读取的数据缓冲区
 * @param  length: 数据长度
 * @retval HAL_StatusTypeDef
 */
static HAL_StatusTypeDef LSM6DSL_ReadReg(uint8_t reg_addr, uint8_t *data, uint16_t length);

/**
 * @brief  LSM6DSL寄存器写入
 * @param  reg_addr: 寄存器地址
 * @param  data: 写入的数据缓冲区
 * @param  length: 数据长度
 * @retval HAL_StatusTypeDef
 */
static HAL_StatusTypeDef LSM6DSL_WriteReg(uint8_t reg_addr, uint8_t *data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* __LSM6DSL_H */