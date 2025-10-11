#include "main.h"
#include "motor_tinyml.h"
#include "esp_adc/adc_oneshot.h"
#include <math.h> // Adicionado para a normalização, se necessário

static const char *TAG = "MAIN_APP";

// Configurações do ADC
#define ADC_CHANNEL ADC_CHANNEL_3 
#define ADC_ATTEN ADC_ATTEN_DB_12 
#define ADC_WIDTH ADC_BITWIDTH_12 
#define ADC_SAMPLES 50

// ==================== PARÂMETROS DE NORMALIZAÇÃO DO MODELO ====================
// Valores fornecidos pelo desenvolvedor do modelo para o StandardScaler
const float SCALER_MEAN[4] = {
    -10138.618036f, // Média do Eixo X (bruto)
    -407.879632f,   // Média do Eixo Y (bruto)
    11927.490799f,  // Média do Eixo Z (bruto)
    3027.813713f    // Média do ADC_RAW
};

const float SCALER_SCALE[4] = {
    1747.075269f,   // Desvio Padrão do Eixo X
    786.377335f,    // Desvio Padrão do Eixo Y
    2424.841654f,   // Desvio Padrão do Eixo Z
    53.533303f      // Desvio Padrão do ADC_RAW
};
// =============================================================================

void app_main(void)
{
    // --- Inicializações ---
    ESP_ERROR_CHECK(MPU_Init());
    ESP_ERROR_CHECK(MotorFaultDetector_Init());

    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_WIDTH,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));
    ESP_LOGI(TAG, "Hardware e Detector inicializados.");

    while (1)
    {
        // --- 1. LEITURA DOS DADOS BRUTOS DOS SENSORES ---
        int16_t accel_x_raw, accel_y_raw, accel_z_raw;
        esp_err_t err = MPU_ReadAccelerometer(&accel_x_raw, &accel_y_raw, &accel_z_raw);
        
        // Média do ADC para filtrar ruído e obter um valor estável
        int adc_sum = 0;
        for (int i = 0; i < ADC_SAMPLES; i++) {
            int adc_raw_single = 0;
            adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw_single);
            adc_sum += adc_raw_single;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        int adc_avg_raw = adc_sum / ADC_SAMPLES;

        if (err == ESP_OK)
        {
            // Array para armazenar os 4 valores de entrada
            float model_input[4] = {
                (float)accel_x_raw,
                (float)accel_y_raw,
                (float)accel_z_raw,
                (float)adc_avg_raw
            };

            // Imprime os dados brutos antes da normalização para depuração
            printf("Dados Brutos -> X:%.0f, Y:%.0f, Z:%.0f, ADC:%.0f\n",
                   model_input[0], model_input[1], model_input[2], model_input[3]);

            // --- 2. NORMALIZAÇÃO DOS DADOS ---
            // Aplica a fórmula (valor - média) / desvio_padrão para cada entrada
            model_input[0] = (model_input[0] - SCALER_MEAN[0]) / SCALER_SCALE[0];
            model_input[1] = (model_input[1] - SCALER_MEAN[1]) / SCALER_SCALE[1];
            model_input[2] = (model_input[2] - SCALER_MEAN[2]) / SCALER_SCALE[2];
            model_input[3] = (model_input[3] - SCALER_MEAN[3]) / SCALER_SCALE[3];

            // Imprime os dados após a normalização para verificar se estão corretos (valores pequenos)
            //printf("Dados Normalizados -> X:%.4f, Y:%.4f, Z:%.4f, ADC:%.4f\n",
            //       model_input[0], model_input[1], model_input[2], model_input[3]);

            // --- 3. EXECUÇÃO DA INFERÊNCIA ---
            // Envia os dados JÁ NORMALIZADOS para o modelo
            MotorFaultDetector_AddSample(model_input[0], model_input[1], model_input[2], model_input[3]);
            
            float confidence[2];
            MotorFaultDetector_Predict(confidence);
            
            // --- 4. EXIBIÇÃO DO RESULTADO ---
            printf("Probabilidade de Falha: %.2f%%\n\n", confidence[0] * 100.0f);
        }

        Delay_ms(tempo(1000));
    }
}