#include "main.h"
#include "motor_tinyml.h"
// #include "esp_adc/adc_oneshot.h" // ADC Removido
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <math.h>

static const char *TAG = "DATA_COLLECTOR";

// --- Configuração dos Pinos ---
#define RELE_PIN            GPIO_NUM_10
#define BOTAO_PIN           GPIO_NUM_7
#define LED_SEM_FALHA       GPIO_NUM_6
#define LED_COM_FALHA       GPIO_NUM_4

// --- Configuração do Encoder ---
// ATENÇÃO: Verifique qual GPIO corresponde ao antigo ADC no seu hardware
#define ENCODER_PIN         GPIO_NUM_5   // <--- SUBSTITUA PELO SEU PINO DE ENTRADA DO ENCODER
#define ENCODER_PPR         20           // <--- Pulsos Por Revolução (Quantos furos tem o disco?)

// --- Configuração I2C Geral ---
#define I2C_MASTER_SCL_IO   9
#define I2C_MASTER_SDA_IO   8
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  100000

#define DEBOUNCE_TIME_MS    50

// --- Offsets ---
typedef struct { float x; float y; float z; } SensorOffsets_t;
SensorOffsets_t OFFSETS_MPU6050 = {150.0f, -940.0f, -1014.0f};
SensorOffsets_t OFFSETS_BMI160  = {850.0f, -610.0f, -374.0f};
SensorOffsets_t *current_offsets = NULL;

// --- Globais e Handles ---
static bool g_user_request_motor = false;
static bool g_sensor_is_valid = false;
static uint8_t g_current_sensor_addr = 0;

// Variáveis do Encoder (Volatile para Interrupção)
static volatile uint32_t g_pulse_count = 0;
static portMUX_TYPE encoder_mux = portMUX_INITIALIZER_UNLOCKED;

SemaphoreHandle_t xI2CMutex;

// ==========================================================
//    INTERRUPÇÃO DO ENCODER
// ==========================================================
static void IRAM_ATTR encoder_isr_handler(void* arg)
{
    // Aumenta o contador de pulsos de forma segura
    portENTER_CRITICAL_ISR(&encoder_mux);
    g_pulse_count++;
    portEXIT_CRITICAL_ISR(&encoder_mux);
}

static void encoder_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE; // Interrupção na borda de subida
    io_conf.pin_bit_mask = (1ULL << ENCODER_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE; // Habilita pullup interno se necessário
    gpio_config(&io_conf);

    // Instala serviço de ISR e adiciona o handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ENCODER_PIN, encoder_isr_handler, (void*) ENCODER_PIN);
    
    ESP_LOGI(TAG, "Encoder configurado no pino %d", ENCODER_PIN);
}

// ==========================================================
//    FUNÇÕES I2C (Bus Scan e Init)
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

uint8_t i2c_scan_bus(void)
{
    for (uint8_t i = 1; i < 127; i++) {
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

// ==========================================================
//    TASK 1: MONITORAMENTO (SENSOR WATCHDOG)
// ==========================================================
void task_sensor_monitor(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) == pdTRUE)
        {
            if (g_sensor_is_valid)
            {
                if (!check_sensor_health(g_current_sensor_addr)) {
                    ESP_LOGE(TAG, "SENSOR DESCONECTADO!");
                    g_sensor_is_valid = false;
                    g_current_sensor_addr = 0;
                    current_offsets = NULL;
                }
            }
            else
            {
                uint8_t addr = i2c_scan_bus();
                if (addr != 0) {
                    ESP_LOGI(TAG, "Sensor detectado em 0x%02X", addr);
                    uint8_t who_am_i = 0;
                    uint8_t reg_check = 0x75;
                    i2c_master_write_read_device(I2C_MASTER_NUM, addr, &reg_check, 1, &who_am_i, 1, pdMS_TO_TICKS(100));

                    bool identified = false;
                    // Tenta MPU
                    if (who_am_i == 0x68 || who_am_i == 0x70) {
                        if (MPU_Init() == ESP_OK) {
                            current_offsets = &OFFSETS_MPU6050;
                            identified = true;
                        }
                    }
                    // Tenta BMI
                    else {
                        reg_check = 0x00;
                        i2c_master_write_read_device(I2C_MASTER_NUM, addr, &reg_check, 1, &who_am_i, 1, pdMS_TO_TICKS(100));
                        if (who_am_i == 0xD1) {
                            if (BMI160_Init() == ESP_OK) {
                                current_offsets = &OFFSETS_BMI160;
                                identified = true;
                            }
                        }
                    }

                    if (identified) {
                        g_current_sensor_addr = addr;
                        g_sensor_is_valid = true;
                        ESP_LOGI(TAG, "Sensor Configurado OK!");
                    }
                }
            }
            xSemaphoreGive(xI2CMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(g_sensor_is_valid ? 500 : 1000));
    }
}

// ==========================================================
//    TASK 2: INTERFACE (BOTÃO/RELÉ)
// ==========================================================
void task_ui_control(void *pvParameters)
{
    uint32_t last_press = 0;
    bool last_relay_state = false;

    while (1)
    {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // Leitura Botão
        if (gpio_get_level(BOTAO_PIN) == 0)
        {
            if ((now - last_press) > DEBOUNCE_TIME_MS)
            {
                last_press = now;
                if (g_sensor_is_valid) {
                    g_user_request_motor = !g_user_request_motor;
                    ESP_LOGI(TAG, "Botao -> Motor: %d", g_user_request_motor);
                } else {
                    // Pisca rápido erro
                    for(int k=0; k<3; k++){
                        gpio_set_level(LED_SEM_FALHA, 1); gpio_set_level(LED_COM_FALHA, 1);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        gpio_set_level(LED_SEM_FALHA, 0); gpio_set_level(LED_COM_FALHA, 0);
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                }
            }
            while (gpio_get_level(BOTAO_PIN) == 0) vTaskDelay(pdMS_TO_TICKS(20));
        }

        // Lógica
        bool should_motor_be_on = g_user_request_motor && g_sensor_is_valid;

        if (g_user_request_motor && !g_sensor_is_valid) {
            g_user_request_motor = false;
            should_motor_be_on = false;
        }

        if (should_motor_be_on != last_relay_state) {
            gpio_set_level(RELE_PIN, should_motor_be_on);
            last_relay_state = should_motor_be_on;
            
            if(!should_motor_be_on) {
                gpio_set_level(LED_SEM_FALHA, 0);
                gpio_set_level(LED_COM_FALHA, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ==========================================================
//    TASK 3: COLETA DE DADOS (RAW + RPM)
// ==========================================================
void task_processing(void *pvParameters)
{
    int16_t ax, ay, az;
    
    // Variáveis para cálculo de RPM
    uint32_t last_calc_time = 0;
    uint32_t current_pulses = 0;
    float rpm = 0.0f;

    while (1)
    {
        // Coleta apenas com Motor ligado e Sensor OK
        if (g_sensor_is_valid && g_user_request_motor)
        {
            esp_err_t err = ESP_FAIL;
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

            // 1. Ler Acelerômetro
            if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) == pdTRUE) {
                if (current_offsets == &OFFSETS_MPU6050) {
                    err = MPU_ReadAccelerometer(&ax, &ay, &az);
                } else if (current_offsets == &OFFSETS_BMI160) {
                    err = BMI160_ReadAccelerometer(&ax, &ay, &az);
                }
                xSemaphoreGive(xI2CMutex);
            }

            // 2. Calcular RPM (baseado no delta de tempo desde a última leitura)
            if (err == ESP_OK)
            {
                // Captura pulsos de forma atômica e reseta contador
                portENTER_CRITICAL(&encoder_mux);
                current_pulses = g_pulse_count;
                g_pulse_count = 0;
                portEXIT_CRITICAL(&encoder_mux);

                uint32_t delta_t = now - last_calc_time;
                if (delta_t > 0) {
                    // Formula: (Pulsos / PPR) * (60000ms / delta_ms)
                    float revs = (float)current_pulses / (float)ENCODER_PPR;
                    rpm = revs * (60000.0f / (float)delta_t);
                }
                last_calc_time = now;

                // 3. Printar Dados Brutos (CSV Style para facilitar coleta)
                // Formato: X_RAW, Y_RAW, Z_RAW, RPM
                printf("DADOS: X=%d | Y=%d | Z=%d | RPM=%.2f\n", ax, ay, az, rpm);
                
                // Feedback visual simples (led verde aceso enquanto coleta)
                gpio_set_level(LED_SEM_FALHA, 1);
                
                /* IA DESATIVADA TEMPORARIAMENTE
                   MotorFaultDetector_AddSample(...)
                   MotorFaultDetector_Predict(...)
                */
            }
            
            // Taxa de amostragem de 100ms (10Hz) para dar tempo de contar pulsos
            // Se o encoder for de baixa resolução, aumente esse delay para ter mais precisão
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
        else
        {
            // Reset de variáveis quando motor desliga
            g_pulse_count = 0;
            last_calc_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

void app_main(void)
{
    // Hardware Init
    ESP_ERROR_CHECK(i2c_bus_init());
    xI2CMutex = xSemaphoreCreateMutex();
    
    // Configura o Encoder
    encoder_init();

    // Configura GPIOs
    gpio_reset_pin(RELE_PIN); gpio_set_direction(RELE_PIN, GPIO_MODE_OUTPUT); gpio_set_level(RELE_PIN, 0);
    gpio_reset_pin(LED_COM_FALHA); gpio_set_direction(LED_COM_FALHA, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LED_SEM_FALHA); gpio_set_direction(LED_SEM_FALHA, GPIO_MODE_OUTPUT);
    gpio_reset_pin(BOTAO_PIN); gpio_set_direction(BOTAO_PIN, GPIO_MODE_INPUT); gpio_set_pull_mode(BOTAO_PIN, GPIO_PULLUP_ONLY);

    // Inicializa modelo (mas não usaremos a predição agora)
    // ESP_ERROR_CHECK(MotorFaultDetector_Init()); 

    // Tasks
    xTaskCreate(task_ui_control, "UI", 4096, NULL, 5, NULL);
    xTaskCreate(task_sensor_monitor, "Monitor", 4096, NULL, 6, NULL);
    xTaskCreate(task_processing, "DataCollect", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Modo Coleta de Dados Iniciado - ADC Substituido por Encoder");
}