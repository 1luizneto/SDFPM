/*
 * Implementação do driver para o sensor MPU6500.
 */
#include "mpu6500.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"
#include "esp_log.h"

// Tag para os logs
static const char *TAG = "MPU6500_DRIVER";

/**
 * @brief Inicializa o barramento I2C como mestre.
 */
static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf =
        {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_MASTER_SDA_IO,
            .scl_io_num = I2C_MASTER_SCL_IO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_MASTER_FREQ_HZ,
        };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

/**
 * @brief Acorda o MPU6500, pois ele inicia em modo de baixo consumo.
 */
esp_err_t MPU6500_Setup(void)
{
    uint8_t check;
    // uint8_t Data;
    esp_err_t status;

    uint8_t who_am_i_reg = MPU6500_WHO_AM_I;
    status = i2c_master_write_read_device(I2C_MASTER_NUM, MPU6500_I2C_ADDRESS, &who_am_i_reg, 1, &check, 1, tempo(1000));

    if (status != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao ler WHO_AM_I: %s", esp_err_to_name(status));
        return status;
    }

    if (check == 0x70 || check == 0x68)
    {
        ESP_LOGI(TAG, "MPU6500 detectado com sucesso! WHO_AM_I: 0x%02X", check);

        Delay_ms(tempo(100));

        status = MPU_Reset();
        if (status != ESP_OK)
        {
            ESP_LOGE(TAG, "Falha ao Resetar %s", esp_err_to_name(status));
            return status;
        }

        status = MPU_ResetDigitalFilters();
        if (status != ESP_OK)
        {
            ESP_LOGE(TAG, "Falha ao Resetar os Filtros Digitais %s", esp_err_to_name(status));
            return status;
        }

        status = MPU_ResetDataRegisters();
        if (status != ESP_OK)
        {
            ESP_LOGE(TAG, "Falha ao Resetar os Registradores de Dado %s", esp_err_to_name(status));
            return status;
        }

        status = MPU_ClockConfig();
        if (status != ESP_OK)
        {
            ESP_LOGE(TAG, "Falha ao Configurar o Clock %s", esp_err_to_name(status));
            return status;
        }

        status = MPU_ConfigSampling_Filtering();
        if (status != ESP_OK)
        {
            ESP_LOGE(TAG, "Falha a amostra e filtro %s", esp_err_to_name(status));
            return status;
        }

        status = MPU_SetGyroscope();
        if (status != ESP_OK)
        {
            ESP_LOGE(TAG, "Falha ao set o valor do giroscópio %s", esp_err_to_name(status));
            return status;
        }

        status = MPU_SetAccelerometer();
        if (status != ESP_OK)
        {
            ESP_LOGE(TAG, "Falha ao set o valor do Acelerômetro %s", esp_err_to_name(status));
            return status;
        }

        status = MPU_SetLPF_Accelerometer();
        if (status != ESP_OK)
        {
            ESP_LOGE(TAG, "Falha ao set o valor de LPF do Acelerômetro %s", esp_err_to_name(status));
            return status;
        }

        status = MPU_SetSmaplingRate();
        if (status != ESP_OK)
        {
            ESP_LOGE(TAG, "Falha ao set o valor Velocidade de Amostras %s", esp_err_to_name(status));
            return status;
        }
    }
    else
    {
        ESP_LOGE(TAG, "MPU Com Defeito! %s", esp_err_to_name(status));
    }

    return status;
}

static esp_err_t MPU_Write(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, MPU6500_I2C_ADDRESS, write_buf, sizeof(write_buf), tempo(1000));
}

esp_err_t MPU_Reset(void)
{
    esp_err_t status = MPU_Write(MPU6500_PWR_MGMT_1, MPU6500_RESET);

    Delay_ms(tempo(100));

    return status;
}

esp_err_t MPU_ResetDigitalFilters(void)
{
    esp_err_t status = MPU_Write(MPU6500_I2C_ADDRESS, 0b00000111);

    Delay_ms(tempo(100));

    return status;
}

esp_err_t MPU_ResetDataRegisters(void)
{
    esp_err_t status = MPU_Write(MPU6500_USER_CRTL, MPU6500_RST_SIGNAL_PATH);

    Delay_ms(tempo(100));

    return status;
}

esp_err_t MPU_ClockConfig(void)
{
    esp_err_t status = MPU_Write(MPU6500_PWR_MGMT_1, 0b001);

    Delay_ms(tempo(100));

    return status;
}

esp_err_t MPU_ConfigSampling_Filtering(void)
{
    esp_err_t status = MPU_Write(MPU6500_CONFIG, 0b100);

    Delay_ms(tempo(100));

    return status;
}

esp_err_t MPU_SetGyroscope(void)
{
    esp_err_t status = MPU_Write(MPU6500_GYRO_CONFIG, 0x00);

    Delay_ms(tempo(100));

    return status;
}

esp_err_t MPU_SetAccelerometer(void)
{
    esp_err_t status = MPU_Write(MPU6500_ACCEL_CONFIG, 0x00);

    Delay_ms(tempo(100));

    return status;
}

esp_err_t MPU_SetLPF_Accelerometer(void)
{
    esp_err_t status = MPU_Write(MPU6500_ACCEL_CONFIG2, 0b100);

    Delay_ms(tempo(100));

    return status;
}

esp_err_t MPU_SetSmaplingRate(void)
{
    esp_err_t status = MPU_Write(MPU6500_SAMPLING_DIV, 9);

    Delay_ms(tempo(100));

    return status;
}

esp_err_t MPU_Init(void)
{
    esp_err_t err = i2c_master_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha na inicialização do I2C: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "I2C inicializado com sucesso!");

    err = MPU6500_Setup();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao acordar o MPU6500: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "MPU6500 acordado com sucesso!");

    return ESP_OK;
}

esp_err_t MPU_ReadAccelerometer(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z)
{
    uint8_t data[6];

    // Lê 6 bytes a partir do registro ACCEL_XOUT_H
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, MPU6500_I2C_ADDRESS, &((uint8_t){MPU6500_ACCEL_XOUT_H}), 1, data, 6, pdMS_TO_TICKS(1000));
    if (err != ESP_OK)
    {
        return err;
    }

    // Combina os bytes High e Low para cada eixo
    *accel_x = (data[0] << 8) | data[1];
    *accel_y = (data[2] << 8) | data[3];
    *accel_z = (data[4] << 8) | data[5];

    return ESP_OK;
}

float MPU_ConverterValue_X(int16_t accel_x_raw)
{
    return accel_x_raw / 16384.0;
}

float MPU_ConverterValue_Y(int16_t accel_y_raw)
{
    return accel_y_raw / 16384.0;
}

float MPU_ConverterValue_Z(int16_t accel_z_raw)
{
    return accel_z_raw / 16384.0;
}