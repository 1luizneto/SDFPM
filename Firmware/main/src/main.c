#include "main.h"
#include "motor_tinyml.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include <math.h>

static const char *TAG = "MAIN_APP";

// --- Configuração dos Pinos ---
#define RELE_PIN    GPIO_NUM_10
#define BOTAO_PIN   GPIO_NUM_7
#define LED_SEM_FALHA GPIO_NUM_4
#define LED_COM_FALHA GPIO_NUM_6

// --- Configurações do Sistema ---
#define ADC_CHANNEL         ADC_CHANNEL_3 
#define ADC_ATTEN           ADC_ATTEN_DB_12 
#define ADC_WIDTH           ADC_BITWIDTH_12 
#define ADC_SAMPLES         50    // Amostras para média do ADC
#define DEBOUNCE_TIME_MS    50    // Tempo (ms) para debounce do botão
#define FAULT_THRESHOLD     0.75f // Limiar de falha (75%). Se a falha passar disso, o relé desliga.

// --- Parâmetros de Normalização ---
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

// --- Variáveis de Estado Globais ---
static int g_system_enabled = 0; // 0=Desligado, 1=Ligado/Monitorando
static uint32_t g_last_button_press_time = 0; // Para debounce
static uint32_t count_print = 0; // Para printar a cada N iterações

void app_main(void)
{
    // --- 1. Inicializações de Periféricos ---
    
    // Sensores e IA
    ESP_ERROR_CHECK(MPU_Init());
    ESP_ERROR_CHECK(MotorFaultDetector_Init());

    // ADC
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_WIDTH,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));

    // Relé (Saída) - Lógica Active-Low
    gpio_reset_pin(RELE_PIN); 
    gpio_set_direction(RELE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELE_PIN, 0); // Estado inicial: 1 (HIGH) = Relé DESLIGADO

    gpio_reset_pin(LED_COM_FALHA); 
    gpio_set_direction(LED_COM_FALHA, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_COM_FALHA, 0);

    gpio_reset_pin(LED_SEM_FALHA); 
    gpio_set_direction(LED_SEM_FALHA, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_SEM_FALHA, 0);

    // Botão (Entrada com Pull-up)
    gpio_reset_pin(BOTAO_PIN);
    gpio_set_direction(BOTAO_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOTAO_PIN, GPIO_PULLUP_ONLY); // Pino lê '1' solto, '0' pressionado

    ESP_LOGI(TAG, "Hardware e Detector inicializados. Pressione o botão para ligar.");

    // --- 2. Loop Principal ---
    while (1)
    {
        // --- Lógica do Botão (Sempre verifica) ---
        if (gpio_get_level(BOTAO_PIN) == 0) // Botão pressionado
        {
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if ((current_time - g_last_button_press_time) > DEBOUNCE_TIME_MS)
            {
                g_last_button_press_time = current_time;
                g_system_enabled = !g_system_enabled; // Inverte o estado (Toggle)

                if (g_system_enabled) {
                    ESP_LOGI(TAG, "Botão Pressionado: SISTEMA LIGADO. Acionando bomba.");
                    gpio_set_level(RELE_PIN, 1); // LIGA o relé (Active-Low)
                } else {
                    ESP_LOGI(TAG, "Botão Pressionado: SISTEMA DESLIGADO. Desligando bomba.");
                    gpio_set_level(LED_SEM_FALHA, 0);
                    gpio_set_level(LED_COM_FALHA, 0);                    
                    gpio_set_level(RELE_PIN, 0); // DESLIGA o relé (Active-Low)
                }
                
                // Espera o botão ser solto
                while(gpio_get_level(BOTAO_PIN) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                ESP_LOGI(TAG, "Botão solto.");
            }
        }

        // --- Lógica de Monitoramento (Só executa se o sistema estiver ligado) ---
        if (g_system_enabled)
        {
            // 1. LEITURA DOS DADOS BRUTOS
            int16_t accel_x_raw, accel_y_raw, accel_z_raw;
            esp_err_t err = MPU_ReadAccelerometer(&accel_x_raw, &accel_y_raw, &accel_z_raw);
            
            int adc_sum = 0;
            for (int i = 0; i < ADC_SAMPLES; i++) {
                int adc_raw_single = 0;
                adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw_single);
                adc_sum += adc_raw_single;
                vTaskDelay(pdMS_TO_TICKS(1)); // Este delay é importante para o watchdog!
            }
            int adc_avg_raw = adc_sum / ADC_SAMPLES;

            if (err == ESP_OK)
            {
                // Prepara os 4 valores brutos
                float model_input[4] = {
                    (float)accel_x_raw,
                    (float)accel_y_raw,
                    (float)accel_z_raw,
                    (float)adc_avg_raw
                };
                
                // 2. NORMALIZAÇÃO
                model_input[0] = (model_input[0] - SCALER_MEAN[0]) / SCALER_SCALE[0];
                model_input[1] = (model_input[1] - SCALER_MEAN[1]) / SCALER_SCALE[1];
                model_input[2] = (model_input[2] - SCALER_MEAN[2]) / SCALER_SCALE[2];
                model_input[3] = (model_input[3] - SCALER_MEAN[3]) / SCALER_SCALE[3];

                // 3. EXECUÇÃO DA INFERÊNCIA
                MotorFaultDetector_AddSample(model_input[0], model_input[1], model_input[2], model_input[3]);
                
                float confidence[2];
                MotorFaultDetector_Predict(confidence);
                float prob_falha = confidence[0];
                
                // 4. AÇÃO E EXIBIÇÃO

                if(count_print >= 20)
                {
                    printf("Monitorando... Probabilidade de Falha: %.2f%%\n", prob_falha * 100.0f);
                    count_print = 0;
                }
                else
                    count_print++;

                if((prob_falha * 100.0f) <= 26.97f)
                {
                    gpio_set_level(LED_SEM_FALHA, 1);
                    gpio_set_level(LED_COM_FALHA, 0);
                }
                else
                {
                    gpio_set_level(LED_SEM_FALHA, 0);
                    gpio_set_level(LED_COM_FALHA, 1);
                }
            }
        }
            Delay_ms(tempo(50));
    }
}