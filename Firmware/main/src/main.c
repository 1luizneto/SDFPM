#include "main.h"
#include "motor_tinyml.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include <math.h>

static const char *TAG = "MAIN_APP";

// --- Configuração dos Pinos ---
#define RELE_PIN    GPIO_NUM_10
#define BOTAO_PIN   GPIO_NUM_7
#define LED_SEM_FALHA GPIO_NUM_6
#define LED_COM_FALHA GPIO_NUM_4

// --- Configuração I2C Geral ---
#define I2C_MASTER_SCL_IO 9       
#define I2C_MASTER_SDA_IO 8       
#define I2C_MASTER_NUM I2C_NUM_0  
#define I2C_MASTER_FREQ_HZ 100000

// --- Configurações do Sistema ---
#define ADC_CHANNEL         ADC_CHANNEL_3 
#define ADC_ATTEN           ADC_ATTEN_DB_12 
#define ADC_WIDTH           ADC_BITWIDTH_12 
#define ADC_SAMPLES         50    
#define DEBOUNCE_TIME_MS    50    
#define FAULT_THRESHOLD     0.75f 

// --- Parâmetros de Normalização ---
const float SCALER_MEAN[4] = { -10138.618036f, -407.879632f, 11927.490799f, 3027.813713f };
const float SCALER_SCALE[4] = { 1747.075269f, 786.377335f, 2424.841654f, 53.533303f };

// --- Offsets ---
typedef struct { float x; float y; float z; } SensorOffsets_t;
 SensorOffsets_t OFFSETS_MPU6050 = {150.0f, -940.0f, -1014.0f};
SensorOffsets_t OFFSETS_BMI160  = {850.0f, -610.0f, -374.0f};
SensorOffsets_t *current_offsets = NULL;

// --- Variáveis Globais de Controle ---
static bool g_motor_on = false;         // O usuário quer o motor ligado?
static bool g_sensor_connected = false; // Existe um sensor validado na linha?
static uint8_t g_current_sensor_addr = 0; // Endereço do sensor atual
static uint32_t g_last_button_press_time = 0;
static uint32_t count_print = 0; 

// ==========================================================
//    FUNÇÕES I2C
// ==========================================================

static esp_err_t i2c_bus_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

// Scanner Low-Level (Não trava com buffer nulo)
uint8_t i2c_scan_bus(void)
{
    for (uint8_t i = 1; i < 127; i++)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t res = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(20));
        i2c_cmd_link_delete(cmd);

        if (res == ESP_OK) return i; 
    }
    return 0; 
}

// Verifica se o sensor atual ainda responde (Ping)
bool check_sensor_health(uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t res = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return (res == ESP_OK);
}

void app_main(void)
{
    // 1. Inicializa I2C (Hardware)
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_LOGI(TAG, "I2C Hardware Initialized");

    // 2. Inicializa outros periféricos
    ESP_ERROR_CHECK(MotorFaultDetector_Init());

    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    adc_oneshot_chan_cfg_t config = { .bitwidth = ADC_WIDTH, .atten = ADC_ATTEN };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));

    gpio_reset_pin(RELE_PIN); gpio_set_direction(RELE_PIN, GPIO_MODE_OUTPUT); gpio_set_level(RELE_PIN, 0);
    gpio_reset_pin(LED_COM_FALHA); gpio_set_direction(LED_COM_FALHA, GPIO_MODE_OUTPUT); gpio_set_level(LED_COM_FALHA, 0);
    gpio_reset_pin(LED_SEM_FALHA); gpio_set_direction(LED_SEM_FALHA, GPIO_MODE_OUTPUT); gpio_set_level(LED_SEM_FALHA, 0);
    gpio_reset_pin(BOTAO_PIN); gpio_set_direction(BOTAO_PIN, GPIO_MODE_INPUT); gpio_set_pull_mode(BOTAO_PIN, GPIO_PULLUP_ONLY);

    ESP_LOGI(TAG, "Sistema iniciado. Aguardando sensor...");
    float aux_x = 0, aux_y = 0, aux_z = 0;

    uint32_t last_scan_time = 0;

    // --- LOOP PRINCIPAL (State Machine) ---
    while (1)
    {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // ============================================================
        // 1. GERENCIAMENTO DE CONEXÃO DO SENSOR (HOT-PLUG)
        // ============================================================
        
        if (g_sensor_connected)
        {
            // Se achamos que está conectado, verificamos se ele ainda responde
            if (!check_sensor_health(g_current_sensor_addr)) 
            {
                ESP_LOGE(TAG, "SENSOR DESCONECTADO ABRUPTAMENTE!");
                g_sensor_connected = false;
                g_current_sensor_addr = 0;
                current_offsets = NULL;
                
                // SEGURANÇA: Se o sensor caiu, desliga o motor imediatamente
                if (g_motor_on) {
                    g_motor_on = false;
                    gpio_set_level(RELE_PIN, 0);
                    gpio_set_level(LED_SEM_FALHA, 0);
                    gpio_set_level(LED_COM_FALHA, 0);
                    ESP_LOGW(TAG, "Motor desligado por seguranca.");
                }
            }
        }
        else
        {
            // Se não está conectado, escaneia periodicamente (ex: a cada 1 seg)
            if ((now - last_scan_time) > 1000) 
            {
                last_scan_time = now;
                uint8_t addr = i2c_scan_bus();
                
                if (addr != 0) 
                {
                    ESP_LOGI(TAG, "Dispositivo detectado em 0x%02X. Tentando identificar...", addr);
                    
                    uint8_t who_am_i = 0;
                    esp_err_t res;
                    bool identified = false;

                    // Tenta MPU (0x75 -> 0x68)
                    uint8_t reg_mpu = 0x75;
                    res = i2c_master_write_read_device(I2C_MASTER_NUM, addr, &reg_mpu, 1, &who_am_i, 1, pdMS_TO_TICKS(100));
                    
                    if (res == ESP_OK && (who_am_i == 0x68 || who_am_i == 0x70 || who_am_i == 0xC8)) {
                        if (MPU_Init() == ESP_OK) {
                            current_offsets = &OFFSETS_MPU6050;
                            identified = true;
                            ESP_LOGI(TAG, "MPU6050 Configurado!");
                        }
                    }

                    // Tenta BMI (0x00 -> 0xD1)
                    if (!identified) {
                        uint8_t reg_bmi = 0x00;
                        res = i2c_master_write_read_device(I2C_MASTER_NUM, addr, &reg_bmi, 1, &who_am_i, 1, pdMS_TO_TICKS(100));
                        
                        if (res == ESP_OK && who_am_i == 0xD1) {
                            if (BMI160_Init() == ESP_OK) {
                                current_offsets = &OFFSETS_BMI160;
                                identified = true;
                                ESP_LOGI(TAG, "BMI160 Configurado!");
                            }
                        }
                    }

                    if (identified) {
                        g_current_sensor_addr = addr;
                        g_sensor_connected = true;
                    } else {
                         ESP_LOGW(TAG, "Sensor desconhecido (ID: 0x%02X)", who_am_i);
                    }
                }
                else {
                    // Sem sensor: Pisca LED vermelho lentamente indicando "Busca"
                    ESP_LOGW(TAG, "Nenhum sensor detectado.");
                }
            }
        }

        // ============================================================
        // 2. LEITURA DO BOTÃO
        // ============================================================
        if (gpio_get_level(BOTAO_PIN) == 0) 
        {
            if ((now - g_last_button_press_time) > DEBOUNCE_TIME_MS)
            {
                g_last_button_press_time = now;
                
                if (g_sensor_connected) 
                {
                    g_motor_on = !g_motor_on; // Alterna estado
                    
                    if (g_motor_on) {
                        ESP_LOGI(TAG, "COMANDO: LIGAR MOTOR");
                        gpio_set_level(RELE_PIN, 1);
                    } else {
                        ESP_LOGI(TAG, "COMANDO: DESLIGAR MOTOR");
                        gpio_set_level(RELE_PIN, 0);
                        gpio_set_level(LED_SEM_FALHA, 0);
                        gpio_set_level(LED_COM_FALHA, 0);
                    }
                } 
                else 
                {
                    ESP_LOGW(TAG, "BOTAO IGNORADO: Nenhum sensor conectado!");
                    // Feedback visual rápido (pisca os dois leds) para dizer "Não posso ligar"
                    for(int k=0; k<3; k++) {
                        gpio_set_level(LED_SEM_FALHA, 1); gpio_set_level(LED_COM_FALHA, 1);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        gpio_set_level(LED_SEM_FALHA, 0); gpio_set_level(LED_COM_FALHA, 0);
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                }

                while(gpio_get_level(BOTAO_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        // ============================================================
        // 3. EXECUÇÃO DA IA (Somente se Motor ON e Sensor OK)
        // ============================================================
        if (g_motor_on && g_sensor_connected)
        {
            int16_t ax, ay, az;
            esp_err_t err = ESP_FAIL;

            // Leitura polimórfica baseada no offset selecionado
            if (current_offsets == &OFFSETS_MPU6050) {
                err = MPU_ReadAccelerometer(&ax, &ay, &az);
            } else if (current_offsets == &OFFSETS_BMI160) {
                err = BMI160_ReadAccelerometer(&ax, &ay, &az);
            }

            // Se der erro de leitura DURANTE a operação, considera desconectado
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Erro de leitura no sensor! Desligando...");
                g_sensor_connected = false; // Isso forçará o desligamento na próxima volta do loop
                continue; 
            }

            // Leitura ADC
            int adc_sum = 0;
            for (int i = 0; i < ADC_SAMPLES; i++) {
                int r; adc_oneshot_read(adc_handle, ADC_CHANNEL, &r); adc_sum += r;
            }
            int adc_avg = adc_sum / ADC_SAMPLES;

            // Processamento IA
            float fx = (float)ax - current_offsets->x;
            float fy = (float)ay - current_offsets->y;
            float fz = (float)az - current_offsets->z;

            float input[4] = { fx, fy, fz, (float)adc_avg };
            for(int i=0; i<4; i++) input[i] = (input[i] - SCALER_MEAN[i]) / SCALER_SCALE[i];

            MotorFaultDetector_AddSample(input[0], input[1], input[2], input[3]);
            float conf[2];
            MotorFaultDetector_Predict(conf);
            
            // Lógica LEDs
            uint32_t falha = (uint32_t)(conf[0] * 100.0f);
            if(falha <= 70) {
                gpio_set_level(LED_SEM_FALHA, 1);
                gpio_set_level(LED_COM_FALHA, 0);
            } else {
                gpio_set_level(LED_SEM_FALHA, 0);
                gpio_set_level(LED_COM_FALHA, 1);
                
                // Opcional: Desligar motor se falha crítica?
                // if (falha > 90) { ... }
            }

            if(count_print >= 10) {
                // printf("Status: ON | Sensor: 0x%02X | Falha: %ld%% | Z Raw: %d\n", g_current_sensor_addr, falha, az);

                aux_x = aux_x / count_print;
                aux_y = aux_y / count_print;
                aux_z = aux_z / count_print;

                printf("X Raw: %.0f | Y Raw: %.0f | Z Raw: %.0f\n", aux_x, aux_y, aux_z);
                printf("X    : %.0f | Y    : %.0f | Z    : %.0f\n\n\n", fx, fy, fz);
                count_print = 0;
                aux_x = aux_y = aux_z = 0;
            } else{
                count_print++;
                aux_x += (float)ax;
                aux_y += (float)ay;
                aux_z += (float)az;
            } 
            
            vTaskDelay(pdMS_TO_TICKS(50)); // Delay de amostragem
        }
        else
        {
            // Sistema em Standby ou Buscando
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}