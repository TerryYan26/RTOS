/**
 ******************************************************************************
 * @file           : lsm6dsl.c
 * @brief          : LSM6DSL IMU传感器驱动程序实现
 * @author         : Your Name
 * @version        : V1.0.0
 * @date           : 2025-11-07
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "lsm6dsl.h"
#include "FreeRTOS.h"
#include "semphr.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define LSM6DSL_TIMEOUT_MS          100
#define LSM6DSL_I2C_ADDRESS         LSM6DSL_I2C_ADDRESS_LOW

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static float accel_sensitivity = 0.061f;   // mg/LSB for ±2g
static float gyro_sensitivity = 8.75f;     // mdps/LSB for ±250dps

/* Private function prototypes -----------------------------------------------*/
static HAL_StatusTypeDef LSM6DSL_ReadReg(uint8_t reg_addr, uint8_t *data, uint16_t length);
static HAL_StatusTypeDef LSM6DSL_WriteReg(uint8_t reg_addr, uint8_t *data, uint16_t length);
static void LSM6DSL_UpdateSensitivity(uint8_t accel_fs, uint8_t gyro_fs);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  初始化LSM6DSL传感器
 * @param  config: 传感器配置参数
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_Init(LSM6DSL_Config_t *config)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint8_t who_am_i = 0;
  uint8_t reg_value = 0;

  /* 检查设备ID */
  status = LSM6DSL_ReadWhoAmI(&who_am_i);
  if (status != HAL_OK || who_am_i != LSM6DSL_WHO_AM_I_VALUE) {
    return HAL_ERROR;
  }

  /* 软件复位 */
  status = LSM6DSL_SoftReset();
  if (status != HAL_OK) {
    return status;
  }

  /* 等待复位完成 */
  vTaskDelay(pdMS_TO_TICKS(10));

  /* 配置加速度计 */
  reg_value = config->accel_odr | config->accel_fs;
  status = LSM6DSL_WriteReg(LSM6DSL_CTRL1_XL, &reg_value, 1);
  if (status != HAL_OK) {
    return status;
  }

  /* 配置陀螺仪 */
  reg_value = config->gyro_odr | config->gyro_fs;
  status = LSM6DSL_WriteReg(LSM6DSL_CTRL2_G, &reg_value, 1);
  if (status != HAL_OK) {
    return status;
  }

  /* 配置控制寄存器3 - 使能BDU (Block Data Update) */
  reg_value = 0x40;  // BDU = 1
  status = LSM6DSL_WriteReg(LSM6DSL_CTRL3_C, &reg_value, 1);
  if (status != HAL_OK) {
    return status;
  }

  /* 更新灵敏度系数 */
  LSM6DSL_UpdateSensitivity(config->accel_fs, config->gyro_fs);

  return HAL_OK;
}

/**
 * @brief  读取LSM6DSL WHO_AM_I寄存器
 * @param  who_am_i: 返回的设备ID
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_ReadWhoAmI(uint8_t *who_am_i)
{
  return LSM6DSL_ReadReg(LSM6DSL_WHO_AM_I, who_am_i, 1);
}

/**
 * @brief  读取LSM6DSL传感器数据
 * @param  data: 返回的传感器数据
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_ReadData(LSM6DSL_Data_t *data)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint8_t raw_data[14] = {0};
  int16_t raw_temp, raw_gyro[3], raw_accel[3];
  uint8_t status_reg = 0;

  /* 检查数据是否准备就绪 */
  status = LSM6DSL_GetStatus(&status_reg);
  if (status != HAL_OK) {
    return status;
  }

  data->data_ready = (status_reg & (LSM6DSL_STATUS_XLDA | LSM6DSL_STATUS_GDA)) ? 1 : 0;
  
  if (!data->data_ready) {
    return HAL_OK;  // 数据未准备就绪，但不是错误
  }

  /* 读取温度和6轴数据 (连续读取) */
  status = LSM6DSL_ReadReg(LSM6DSL_OUT_TEMP_L, raw_data, 14);
  if (status != HAL_OK) {
    return status;
  }

  /* 解析原始数据 */
  raw_temp = (int16_t)((raw_data[1] << 8) | raw_data[0]);
  raw_gyro[0] = (int16_t)((raw_data[3] << 8) | raw_data[2]);   // X轴陀螺仪
  raw_gyro[1] = (int16_t)((raw_data[5] << 8) | raw_data[4]);   // Y轴陀螺仪
  raw_gyro[2] = (int16_t)((raw_data[7] << 8) | raw_data[6]);   // Z轴陀螺仪
  raw_accel[0] = (int16_t)((raw_data[9] << 8) | raw_data[8]);  // X轴加速度计
  raw_accel[1] = (int16_t)((raw_data[11] << 8) | raw_data[10]); // Y轴加速度计
  raw_accel[2] = (int16_t)((raw_data[13] << 8) | raw_data[12]); // Z轴加速度计

  /* 转换为实际物理量 */
  data->temperature = 25.0f + (float)raw_temp / 256.0f;  // °C

  /* 加速度 (m/s²) */
  data->accel_x = (float)raw_accel[0] * accel_sensitivity * 9.80665f / 1000.0f;
  data->accel_y = (float)raw_accel[1] * accel_sensitivity * 9.80665f / 1000.0f;
  data->accel_z = (float)raw_accel[2] * accel_sensitivity * 9.80665f / 1000.0f;

  /* 角速度 (rad/s) */
  data->gyro_x = (float)raw_gyro[0] * gyro_sensitivity * 3.14159f / (180.0f * 1000.0f);
  data->gyro_y = (float)raw_gyro[1] * gyro_sensitivity * 3.14159f / (180.0f * 1000.0f);
  data->gyro_z = (float)raw_gyro[2] * gyro_sensitivity * 3.14159f / (180.0f * 1000.0f);

  /* 时间戳 */
  data->timestamp = xTaskGetTickCount();

  return HAL_OK;
}

/**
 * @brief  检查数据是否准备就绪
 * @param  status: 返回的状态寄存器值
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_GetStatus(uint8_t *status)
{
  return LSM6DSL_ReadReg(LSM6DSL_STATUS_REG, status, 1);
}

/**
 * @brief  软件复位LSM6DSL
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_SoftReset(void)
{
  uint8_t reg_value = 0x01;  // SW_RESET = 1
  return LSM6DSL_WriteReg(LSM6DSL_CTRL3_C, &reg_value, 1);
}

/**
 * @brief  使能/禁用LSM6DSL
 * @param  enable: 1-使能, 0-禁用
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_Enable(uint8_t enable)
{
  HAL_StatusTypeDef status = HAL_OK;
  uint8_t reg_value = 0;

  if (enable) {
    /* 使能加速度计 - 104Hz */
    reg_value = LSM6DSL_XL_ODR_104_HZ | LSM6DSL_XL_FS_2G;
    status = LSM6DSL_WriteReg(LSM6DSL_CTRL1_XL, &reg_value, 1);
    if (status != HAL_OK) return status;

    /* 使能陀螺仪 - 104Hz */
    reg_value = LSM6DSL_GY_ODR_104_HZ | LSM6DSL_GY_FS_250_DPS;
    status = LSM6DSL_WriteReg(LSM6DSL_CTRL2_G, &reg_value, 1);
  } else {
    /* 禁用加速度计 */
    reg_value = LSM6DSL_XL_ODR_POWER_DOWN;
    status = LSM6DSL_WriteReg(LSM6DSL_CTRL1_XL, &reg_value, 1);
    if (status != HAL_OK) return status;

    /* 禁用陀螺仪 */
    reg_value = LSM6DSL_GY_ODR_POWER_DOWN;
    status = LSM6DSL_WriteReg(LSM6DSL_CTRL2_G, &reg_value, 1);
  }

  return status;
}

/**
 * @brief  配置LSM6DSL中断
 * @param  int_config: 中断配置
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef LSM6DSL_ConfigInterrupt(uint8_t int_config)
{
  uint8_t reg_value = int_config;
  return LSM6DSL_WriteReg(LSM6DSL_CTRL4_C, &reg_value, 1);
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  LSM6DSL寄存器读取
 * @param  reg_addr: 寄存器地址
 * @param  data: 读取的数据缓冲区
 * @param  length: 数据长度
 * @retval HAL_StatusTypeDef
 */
static HAL_StatusTypeDef LSM6DSL_ReadReg(uint8_t reg_addr, uint8_t *data, uint16_t length)
{
  HAL_StatusTypeDef status = HAL_OK;
  
  /* 获取I2C互斥锁 */
  if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(LSM6DSL_TIMEOUT_MS)) == pdTRUE) {
    /* I2C读取 */
    status = HAL_I2C_Mem_Read(&hi2c2, LSM6DSL_I2C_ADDRESS << 1, reg_addr, 
                              I2C_MEMADD_SIZE_8BIT, data, length, LSM6DSL_TIMEOUT_MS);
    
    /* 释放I2C互斥锁 */
    xSemaphoreGive(xI2CMutex);
  } else {
    status = HAL_TIMEOUT;
  }

  return status;
}

/**
 * @brief  LSM6DSL寄存器写入
 * @param  reg_addr: 寄存器地址
 * @param  data: 写入的数据缓冲区
 * @param  length: 数据长度
 * @retval HAL_StatusTypeDef
 */
static HAL_StatusTypeDef LSM6DSL_WriteReg(uint8_t reg_addr, uint8_t *data, uint16_t length)
{
  HAL_StatusTypeDef status = HAL_OK;
  
  /* 获取I2C互斥锁 */
  if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(LSM6DSL_TIMEOUT_MS)) == pdTRUE) {
    /* I2C写入 */
    status = HAL_I2C_Mem_Write(&hi2c2, LSM6DSL_I2C_ADDRESS << 1, reg_addr, 
                               I2C_MEMADD_SIZE_8BIT, data, length, LSM6DSL_TIMEOUT_MS);
    
    /* 释放I2C互斥锁 */
    xSemaphoreGive(xI2CMutex);
  } else {
    status = HAL_TIMEOUT;
  }

  return status;
}

/**
 * @brief  根据满量程配置更新灵敏度系数
 * @param  accel_fs: 加速度计满量程配置
 * @param  gyro_fs: 陀螺仪满量程配置
 * @retval None
 */
static void LSM6DSL_UpdateSensitivity(uint8_t accel_fs, uint8_t gyro_fs)
{
  /* 更新加速度计灵敏度 (mg/LSB) */
  switch (accel_fs) {
    case LSM6DSL_XL_FS_2G:
      accel_sensitivity = 0.061f;
      break;
    case LSM6DSL_XL_FS_4G:
      accel_sensitivity = 0.122f;
      break;
    case LSM6DSL_XL_FS_8G:
      accel_sensitivity = 0.244f;
      break;
    case LSM6DSL_XL_FS_16G:
      accel_sensitivity = 0.488f;
      break;
    default:
      accel_sensitivity = 0.061f;
      break;
  }

  /* 更新陀螺仪灵敏度 (mdps/LSB) */
  switch (gyro_fs) {
    case LSM6DSL_GY_FS_125_DPS:
      gyro_sensitivity = 4.375f;
      break;
    case LSM6DSL_GY_FS_250_DPS:
      gyro_sensitivity = 8.75f;
      break;
    case LSM6DSL_GY_FS_500_DPS:
      gyro_sensitivity = 17.50f;
      break;
    case LSM6DSL_GY_FS_1000_DPS:
      gyro_sensitivity = 35.0f;
      break;
    case LSM6DSL_GY_FS_2000_DPS:
      gyro_sensitivity = 70.0f;
      break;
    default:
      gyro_sensitivity = 8.75f;
      break;
  }
}