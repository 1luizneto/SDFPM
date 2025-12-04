/*
 * Driver para o sensor BMI160 utilizando I2C.
 * Compatível com a estrutura do MPU6500.
 */
#ifndef BMI160_H
#define BMI160_H

#include <stdint.h>
#include "esp_err.h"

// Reutiliza definições de tempo do FreeRTOS se já não estiverem definidas
#ifndef Delay_ms
#define Delay_ms vTaskDelay
#endif
#ifndef tempo
#define tempo pdMS_TO_TICKS
#endif

// --- Definições do BMI160 ---
// Endereço I2C (Pode ser 0x68 ou 0x69 dependendo do pino SDO)
#define BMI160_I2C_ADDRESS  0x69 

// Registradores Importantes
#define BMI160_CHIP_ID_REG  0x00 // Deve retornar 0xD1
#define BMI160_CMD_REG      0x7E // Registrador de comandos
#define BMI160_ACC_CONF     0x40 // Configuração (ODR/Bandwidth)
#define BMI160_ACC_RANGE    0x41 // Configuração de Escala (2g, 4g, etc)
#define BMI160_DATA_14      0x12 // Início dos dados de Aceleração (LSB do X)

// Comandos para o registrador CMD (0x7E)
#define BMI160_CMD_SOFT_RESET    0xB6
#define BMI160_CMD_ACC_NORMAL    0x11 // Liga o Acelerômetro (Modo Normal)

// Valores de Configuração
#define BMI160_ACC_RANGE_2G      0x03 // ±2g (Igual ao padrão do MPU)
#define BMI160_ACC_ODR_100HZ     0x28 // Output Data Rate ~100Hz

// Definições do barramento I2C (Mesmos do MPU para compatibilidade)
#define I2C_MASTER_SCL_IO 9       
#define I2C_MASTER_SDA_IO 8       
#define I2C_MASTER_NUM I2C_NUM_0  
#define I2C_MASTER_FREQ_HZ 100000 
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0

/**
 * @brief Inicializa o BMI160.
 * Verifica o ID, reseta, liga o acelerômetro e configura para ±2g.
 */
esp_err_t BMI160_Init(void);

/**
 * @brief Lê os dados brutos do acelerômetro.
 * IMPORTANTE: O BMI160 envia LSB primeiro, depois MSB (diferente do MPU).
 */
esp_err_t BMI160_ReadAccelerometer(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z);

// Funções auxiliares de setup interno
esp_err_t BMI160_Setup(void);

#endif // BMI160_H