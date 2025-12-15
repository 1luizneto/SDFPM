#include "main.h"
#include "motor_tinyml.h"
// #include "esp_adc/adc_oneshot.h" // ADC Removido
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <math.h>

// --- INCLUSÃO DO MÓDULO WI-FI ---
#include "wifi.h"

static const char *TAG = "DATA_COLLECTOR";

// --- Configuração dos Pinos ---
#define RELE_PIN            GPIO_NUM_10
#define BOTAO_PIN           GPIO_NUM_7
#define LED_SEM_FALHA       GPIO_NUM_6
#define LED_COM_FALHA       GPIO_NUM_4

// --- Configuração do Encoder ---
#define ENCODER_PIN         GPIO_NUM_5   // Verifique seu hardware
#define ENCODER_PPR         20           // Pulsos por revolução

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
int count_print = 0;

// --- Estrutura para Compartilhar Dados entre Tasks ---
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    float rpm;
    bool is_fault; // <--- ADICIONE ESTE CAMPO
} SystemData_t;

// --- Globais e Handles ---
static bool g_user_request_motor = false;
static bool g_sensor_is_valid = false;
static uint8_t g_current_sensor_addr = 0;

// Variáveis do Encoder (Volatile para Interrupção)
static volatile uint32_t g_pulse_count = 0;
static portMUX_TYPE encoder_mux = portMUX_INITIALIZER_UNLOCKED;

// Mutexes
SemaphoreHandle_t xI2CMutex;  // Protege Hardware I2C
SemaphoreHandle_t xDataMutex; // Protege leitura/escrita dos dados (SystemData_t)

// Dados Compartilhados (Última leitura válida)
SystemData_t g_latest_data = {0};

// ==========================================================
//    INTERRUPÇÃO DO ENCODER
// ==========================================================
static void IRAM_ATTR encoder_isr_handler(void* arg)
{
    portENTER_CRITICAL_ISR(&encoder_mux);
    g_pulse_count++;
    portEXIT_CRITICAL_ISR(&encoder_mux);
}

static void encoder_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = (1ULL << ENCODER_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(ENCODER_PIN, encoder_isr_handler, (void*) ENCODER_PIN);
}

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
                    if (who_am_i == 0x68 || who_am_i == 0x70) {
                        if (MPU_Init() == ESP_OK) {
                            current_offsets = &OFFSETS_MPU6050;
                            identified = true;
                        }
                    }
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
//    TASK 3: COLETA DE DADOS (10Hz - Rápida)
//    Atualiza a estrutura global, mas NÃO envia Wi-Fi
// ==========================================================
void task_processing(void *pvParameters)
{
    int16_t ax, ay, az;
    uint32_t last_calc_time = 0;
    uint32_t current_pulses = 0;
    float rpm = 0.0f;

    while (1)
    {
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

            // 2. Calcular RPM e Atualizar Globais
            if (err == ESP_OK)
            {
                portENTER_CRITICAL(&encoder_mux);
                current_pulses = g_pulse_count;
                g_pulse_count = 0;
                portEXIT_CRITICAL(&encoder_mux);

                uint32_t delta_t = now - last_calc_time;
                if (delta_t > 0) {
                    float revs = (float)current_pulses / (float)ENCODER_PPR;
                    rpm = revs * (60000.0f / (float)delta_t);
                }
                last_calc_time = now;

                if (rpm <= 1000) rpm = 0;

                // --- 3. PREPARAÇÃO E INFERÊNCIA ---
                
                // A. Aplica Offsets (Isso continua aqui pois depende do sensor físico)
                float fx = (float)ax - current_offsets->x;
                float fy = (float)ay - current_offsets->y;
                float fz = (float)az - current_offsets->z;

                // B. Executa IA
                // Nota: Passamos fx, fy, fz, rpm. A normalização (Scaler) agora ocorre dentro da função!
                MotorFaultDetector_AddSample(fx, fy, fz, rpm);
                
                float conf[5]; // Mudado para 5 classes
                int class_idx = MotorFaultDetector_Predict(conf);
                
                // C. Classificação de Falha (5 Classes)
                bool is_fault = false;
                
                // Verifica contra o novo ENUM
                if (class_idx == CLASS_FALHA_1 || class_idx == CLASS_FALHA_2 || class_idx == CLASS_FALHA_3) { 
                    is_fault = true;
                }

                // D. Atualiza LEDs
                if (is_fault) {
                    gpio_set_level(LED_SEM_FALHA, 0);
                    gpio_set_level(LED_COM_FALHA, 1); // Vermelho
                } else {
                    gpio_set_level(LED_SEM_FALHA, 1); // Verde
                    gpio_set_level(LED_COM_FALHA, 0);
                }

                // Log de Debug

                //printf("X=%.0f | Y=%.0f | Z=%.0f | RPM=%.2f\n", fx, fy, fz, rpm);

                // Ajuste os nomes conforme a ordem real do seu treinamento!
                const char* class_names[] = {"OFF", "FALHA 1", "FALHA 2", "FALHA 3", "NORMAL"}; 

                // --- LÓGICA DE VOTAÇÃO (BUFFER DE 10) ---
                static int pred_buffer[10]; // Buffer estático para guardar o histórico
                
                // Guarda a predição atual no buffer
                if (count_print < 10) {
                    pred_buffer[count_print] = class_idx;
                }
                count_print++;
                
                if(count_print >= 10) {
                    // 1. Contabiliza os votos
                    int votos[5] = {0}; // Zera contadores para as 5 classes
                    for(int i=0; i<10; i++) {
                        if(pred_buffer[i] >= 0 && pred_buffer[i] < 5) {
                            votos[pred_buffer[i]]++;
                        }
                    }

                    // 2. Descobre o vencedor (Moda)
                    int winner_idx = 0;
                    int max_votos = 0;
                    for(int i=0; i<5; i++) {
                        if(votos[i] > max_votos) {
                            max_votos = votos[i];
                            winner_idx = i;
                        }
                    }

                    if (winner_idx == 4 && max_votos <= 6) {
                        winner_idx = 2; // 2 é o índice de CLASS_FALHA_2
                        max_votos = 0;  
                    }

                    // 3. Printa a classe vencedora e a "confiança" baseada nos votos (ex: 8/10 = 80%)
                    printf("Sensor: %s | IA (Moda 10): %s (Votos: %d/10) | RPM: %.0f\n", 
                           (current_offsets == &OFFSETS_MPU6050) ? "MPU" : "BMI", 
                           class_names[winner_idx], 
                           max_votos, 
                           rpm);
                           
                    count_print = 0;
                }


                // --- 4. ATUALIZAÇÃO DOS DADOS GLOBAIS ---
                if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    g_latest_data.x = ax;
                    g_latest_data.y = ay;
                    g_latest_data.z = az;
                    g_latest_data.rpm = rpm;
                    g_latest_data.is_fault = is_fault;
                    xSemaphoreGive(xDataMutex);
                }
            }
            
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
        else
        {
            g_pulse_count = 0;
            last_calc_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            // Zera dados globais se motor desligado
            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_latest_data.x = 0; g_latest_data.y = 0; g_latest_data.z = 0; g_latest_data.rpm = 0;
                xSemaphoreGive(xDataMutex);
            }
            
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

// ==========================================================
//    TASK 4: TELEMETRIA (1Hz - Lenta)
//    Responsável apenas por enviar o HTTP
// ==========================================================
void task_telemetry(void *pvParameters)
{
    // Espera o Wi-Fi inicializar
    vTaskDelay(pdMS_TO_TICKS(3000));

    SystemData_t snapshot;

    while (1)
    {
        // Só envia se o motor estiver LIGADO (usuário pediu + sensor ok)
        // E se o Wi-Fi estiver conectado
        if (g_user_request_motor && g_sensor_is_valid && wifi_app_check_connection())
        {
            // 1. Copia os dados mais recentes para uma variável local (Snapshot)
            // Isso libera o Mutex rápido para a task de coleta não travar
            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                snapshot = g_latest_data;
                xSemaphoreGive(xDataMutex);
                
                // 2. Envia HTTP (Isso é lento, pode demorar 1-2s)
                // "false" no final é o campo "em_falha", fixo por enquanto
                esp_err_t err = wifi_send_telemetry(snapshot.x, snapshot.y, snapshot.z, snapshot.rpm, false);
                
                if (err == ESP_OK) {
                     // printf("Telemetria enviada: RPM %.1f\n", snapshot.rpm);
                }
            }
        }
        
        // Intervalo de envio (1 segundo)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    // Hardware Init
    ESP_ERROR_CHECK(i2c_bus_init());

    ESP_ERROR_CHECK(MotorFaultDetector_Init());
    
    // Cria Mutexes
    xI2CMutex = xSemaphoreCreateMutex();
    xDataMutex = xSemaphoreCreateMutex();
    
    // Configura o Encoder
    encoder_init();

    // Inicializa Wi-Fi (NVS + Station)
    wifi_app_init();

    // Configura GPIOs
    gpio_reset_pin(RELE_PIN); gpio_set_direction(RELE_PIN, GPIO_MODE_OUTPUT); gpio_set_level(RELE_PIN, 0);
    gpio_reset_pin(LED_COM_FALHA); gpio_set_direction(LED_COM_FALHA, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LED_SEM_FALHA); gpio_set_direction(LED_SEM_FALHA, GPIO_MODE_OUTPUT);
    gpio_reset_pin(BOTAO_PIN); gpio_set_direction(BOTAO_PIN, GPIO_MODE_INPUT); gpio_set_pull_mode(BOTAO_PIN, GPIO_PULLUP_ONLY);

    // Tasks
    // 1. UI: Responde ao botão instantaneamente
    xTaskCreate(task_ui_control, "UI", 4096, NULL, 5, NULL);
    
    // 2. Monitor: Garante que I2C está vivo
    xTaskCreate(task_sensor_monitor, "Monitor", 4096, NULL, 6, NULL);
    
    // 3. Coleta: Lê sensores e calcula RPM a 10Hz
    xTaskCreate(task_processing, "DataCollect", 4096, NULL, 5, NULL);
    
    // 4. Telemetria: Envia dados para o PC a 1Hz
    // Stack maior (8192) pois HTTP/TLS consome muita memória

    //COMENTADO PARA GRAVAR OS DADOS LOCALMENTE
    //xTaskCreate(task_telemetry, "Telemetry", 8192, NULL, 4, NULL);

    ESP_LOGI(TAG, "Sistema Completo Iniciado: Motor + Encoder + Wi-Fi Telemetria");
}