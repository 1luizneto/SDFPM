/*
 * Projeto de Embarcados e Prototipagem 0.0.1
 *
 * Autores: Luiz Neto, Gabriel Domingos, Ryan Soares e Raul Confessor
 */

#include "main.h"

static const char *TAG = "MAIN_APP";

void app_main(void)
{
    // 1. Inicializa o driver do MPU6500 (que por sua vez inicializa o I2C)
    ESP_ERROR_CHECK(MPU_Init());
    ESP_LOGI(TAG, "Driver do MPU6500 inicializado.");

    int16_t accel_x_raw, accel_y_raw, accel_z_raw;
    float accel_x, accel_y, accel_z;

    while (1)
    {
        // 2. Lê os dados do sensor usando a função do driver
        esp_err_t err = MPU_ReadAccelerometer(&accel_x_raw, &accel_y_raw, &accel_z_raw);

        accel_x = MPU_ConverterValue_X(accel_x_raw);
        accel_y = MPU_ConverterValue_Y(accel_y_raw);
        accel_z = MPU_ConverterValue_Z(accel_z_raw);

        if (err == ESP_OK)
        {
            // 3. Imprime os dados lidos
            printf("Accel X: %.2f \t Accel Y: %.2f \t Accel Z: %.2f\n", accel_x, accel_y, accel_z);
        }
        else
        {
            ESP_LOGE(TAG, "Falha ao ler o acelerômetro: %s", esp_err_to_name(err));
        }

        Delay_ms(tempo(500));
    }
}