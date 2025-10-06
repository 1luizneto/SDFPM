/*
 * Projeto de Embarcados e Prototipagem 0.0.1
 *
 * Autores: Luiz Neto, Gabriel Domingos, Ryan Soares e Raul Confessor
 */

#include "main.h"
#include "motor_tinyml.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "MAIN_APP";

// Configurações do ADC
#define ADC_CHANNEL ADC_CHANNEL_3 // GPIO4 = ADC1_CHANNEL_4
#define ADC_ATTEN ADC_ATTEN_DB_12 // Atenuação 12dB (0-3.6V)
#define ADC_WIDTH ADC_BITWIDTH_12 // Resolução 12 bits (0-4095)
#define ADC_SAMPLES 50            // Número de amostras para média

void app_main(void)
{
    // 1. Inicializa o driver do MPU6500 (que por sua vez inicializa o I2C)
    ESP_ERROR_CHECK(MPU_Init());
    ESP_LOGI(TAG, "Driver do MPU6500 inicializado.");

    // 2. Inicializa o detector de falhas
    ESP_ERROR_CHECK(MotorFaultDetector_Init());
    ESP_LOGI(TAG, "Detector de falhas inicializado.");

    // 3. Configura o ADC para leitura do sensor de corrente (Nova API)
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_WIDTH,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));
    ESP_LOGI(TAG, "ADC configurado no GPIO4 (ADC1_CH3).");

    // 4. Calibração do ADC (Nova API)
    adc_cali_handle_t adc_cali_handle = NULL;
    bool do_calibration = false;

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_WIDTH,
    };
    esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle);
    if (ret == ESP_OK)
    {
        do_calibration = true;
        ESP_LOGI(TAG, "Calibração do ADC habilitada.");
    }
    else
    {
        ESP_LOGW(TAG, "Calibração falhou: %s", esp_err_to_name(ret));
    }

    int16_t accel_x_raw, accel_y_raw, accel_z_raw;

    while (1)
    {
        esp_err_t err = MPU_ReadAccelerometer(&accel_x_raw, &accel_y_raw, &accel_z_raw);

        if (err == ESP_OK)
        {
            // Lê o ADC múltiplas vezes e faz média
            int adc_sum = 0;
            for (int i = 0; i < ADC_SAMPLES; i++)
            {
                int adc_raw_single = 0;
                adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw_single);
                adc_sum += adc_raw_single;
                vTaskDelay(pdMS_TO_TICKS(2)); // Delay entre leituras
            }
            int adc_raw = adc_sum / ADC_SAMPLES;

            // Converte para mV
            int adc_mv = 0;
            if (do_calibration)
            {
                adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &adc_mv);
            }
            else
            {
                // Conversão manual: ADC_ATTEN_DB_12 -> 0-3600mV
                adc_mv = (adc_raw * 3600) / 4095;
            }

            // Calcula tensão em Volts
            float voltage = adc_mv / 1000.0f;

            printf("X %d;Y %d;Z %d;ADC_RAW %d;ADC_MV %d;VOLTAGE %.3fV\n",
                   accel_x_raw, accel_y_raw, accel_z_raw, adc_raw, adc_mv, voltage);
        }

        Delay_ms(tempo(20));
    }
}