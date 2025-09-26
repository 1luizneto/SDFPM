/*
 * Driver para o sensor MPU6500 utilizando I2C.
 */
#ifndef MPU6500_H
#define MPU6500_H

#include <stdint.h>
#include "esp_err.h"

#define Delay_ms vTaskDelay
#define tempo pdMS_TO_TICKS

// Definições do MPU6500
#define MPU6500_I2C_ADDRESS 0x68  // Endereço I2C do MPU6500
#define MPU6500_PWR_MGMT_1 0x6B   // Registro de gerenciamento de energia
#define MPU6500_ACCEL_XOUT_H 0x3B // Registro do dado X do acelerômetro (High Byte)
#define MPU6500_WHO_AM_I 0x75     // Registro WHO_AM_I
#define MPU6500_USER_CRTL 0x6A
#define MPU6500_CONFIG 0x1A
#define MPU6500_GYRO_CONFIG 0x1B
#define MPU6500_ACCEL_CONFIG 0x1C
#define MPU6500_ACCEL_CONFIG2 0x1D
#define MPU6500_SAMPLING_DIV 0x19

#define MPU6500_RESET (uint8_t)(1 << 7) /*< Bit 7: Device reset (PWR_MGMT_1). */
#define MPU6500_RST_SIGNAL_PATH (uint8_t)(1 << 0)

// Definições do barramento I2C
#define I2C_MASTER_SCL_IO 9       // GPIO para o SCL
#define I2C_MASTER_SDA_IO 8       // GPIO para o SDA
#define I2C_MASTER_NUM I2C_NUM_0  // Porta I2C a ser usada
#define I2C_MASTER_FREQ_HZ 100000 // Frequência do clock I2C
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

/**
 * @brief Inicializa o barramento I2C e o sensor MPU6500.
 * Esta função deve ser chamada uma vez antes de ler os dados.
 *
 * @return esp_err_t ESP_OK em caso de sucesso, ou um código de erro.
 */
esp_err_t MPU_Init(void);

/**
 * @brief Lê os dados brutos do acelerômetro do MPU6500.
 *
 * @param accel_x Ponteiro para armazenar o valor do eixo X.
 * @param accel_y Ponteiro para armazenar o valor do eixo Y.
 * @param accel_z Ponteiro para armazenar o valor do eixo Z.
 * @return esp_err_t ESP_OK em caso de sucesso, ou um código de erro.
 */
esp_err_t MPU_ReadAccelerometer(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z);

float MPU_ConverterValue_X(int16_t accel_x_raw);
float MPU_ConverterValue_Y(int16_t accel_y_raw);
float MPU_ConverterValue_Z(int16_t accel_z_raw);

esp_err_t MPU6500_Setup(void);
esp_err_t MPU_Reset(void);
esp_err_t MPU_ResetDigitalFilters(void);
esp_err_t MPU_ResetDataRegisters(void);
esp_err_t MPU_ClockConfig(void);
esp_err_t MPU_ConfigSampling_Filtering(void);
esp_err_t MPU_SetGyroscope(void);
esp_err_t MPU_SetAccelerometer(void);
esp_err_t MPU_SetLPF_Accelerometer(void);
esp_err_t MPU_SetSmaplingRate(void);

#endif // MPU6500_H