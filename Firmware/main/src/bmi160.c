/*
 * Implementação do driver para o sensor BMI160.
 */
#include "bmi160.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "BMI160_DRIVER";

/**
 * @brief Função auxiliar para escrever em registrador
 */
static esp_err_t BMI160_Write(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, BMI160_I2C_ADDRESS, write_buf, sizeof(write_buf), tempo(1000));
}

/**
 * @brief Configura o BMI160 (Reset, Power, Range)
 */
esp_err_t BMI160_Setup(void)
{
    uint8_t chip_id;
    esp_err_t status;
    uint8_t reg_addr = BMI160_CHIP_ID_REG;

    // 1. Verificar Chip ID
    status = i2c_master_write_read_device(I2C_MASTER_NUM, BMI160_I2C_ADDRESS, &reg_addr, 1, &chip_id, 1, tempo(1000));

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Erro I2C ao ler BMI160: %s", esp_err_to_name(status));
        return status;
    }

    if (chip_id == 0xD1) {
        ESP_LOGI(TAG, "BMI160 detectado! Chip ID: 0x%02X", chip_id);

        // 2. Soft Reset para garantir estado limpo
        BMI160_Write(BMI160_CMD_REG, BMI160_CMD_SOFT_RESET);
        Delay_ms(tempo(50)); // Espera o sensor reiniciar

        // 3. LIGAR O ACELERÔMETRO (Crucial: BMI160 inicia em Suspend)
        status = BMI160_Write(BMI160_CMD_REG, BMI160_CMD_ACC_NORMAL);
        if (status != ESP_OK) return status;
        
        // Espera o acelerômetro acordar (datasheet diz ~3.8ms, damos 50ms por segurança)
        Delay_ms(tempo(50)); 

        // 4. Configurar Range para ±2g (Igual ao MPU)
        // Isso garante que 1g = 16384 LSB
        status = BMI160_Write(BMI160_ACC_RANGE, BMI160_ACC_RANGE_2G);
        if (status != ESP_OK) return status;

        // 5. Configurar Data Rate (ODR) para 100Hz (BWP Normal)
        status = BMI160_Write(BMI160_ACC_CONF, BMI160_ACC_ODR_100HZ);
        if (status != ESP_OK) return status;

        ESP_LOGI(TAG, "BMI160 Configurado (Normal Mode, +/-2g, 100Hz)");
    } else {
        ESP_LOGE(TAG, "Chip ID incorreto: 0x%02X (Esperado 0xD1)", chip_id);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t BMI160_Init(void)
{
    return BMI160_Setup();
}

esp_err_t BMI160_ReadAccelerometer(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z)
{
    uint8_t data[6];
    uint8_t reg_addr = BMI160_DATA_14; // Começa lendo do registro 0x12

    // Lê 6 bytes sequenciais:
    // data[0] = X_LSB, data[1] = X_MSB
    // data[2] = Y_LSB, data[3] = Y_MSB
    // data[4] = Z_LSB, data[5] = Z_MSB
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, BMI160_I2C_ADDRESS, &reg_addr, 1, data, 6, pdMS_TO_TICKS(100));
    
    if (err != ESP_OK) {
        return err;
    }

    // --- DIFERENÇA CRUCIAL PARA O MPU ---
    // O BMI160 é Little Endian (LSB no endereço menor)
    *accel_x = (int16_t)((data[1] << 8) | data[0]);
    *accel_y = (int16_t)((data[3] << 8) | data[2]);
    *accel_z = (int16_t)((data[5] << 8) | data[4]);

    return ESP_OK;
}