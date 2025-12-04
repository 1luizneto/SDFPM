#include "main.h"
#include "motor_tinyml.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h" // Necessário para o Mutex
#include <math.h>

static const char *TAG = "MAIN_APP";

// --- Configuração dos Pinos ---
#define RELE_PIN GPIO_NUM_10
#define BOTAO_PIN GPIO_NUM_7
#define LED_SEM_FALHA GPIO_NUM_6
#define LED_COM_FALHA GPIO_NUM_4

// --- Configuração I2C Geral ---
#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

// --- Configurações do Sistema ---
#define ADC_CHANNEL ADC_CHANNEL_3
#define ADC_ATTEN ADC_ATTEN_DB_12
#define ADC_WIDTH ADC_BITWIDTH_12
#define ADC_SAMPLES 50
#define DEBOUNCE_TIME_MS 50

// --- Parâmetros de Normalização ---
const float SCALER_MEAN[4] = {-10138.618036f, -407.879632f, 11927.490799f, 3027.813713f};
const float SCALER_SCALE[4] = {1747.075269f, 786.377335f, 2424.841654f, 53.533303f};

// --- Offsets ---
typedef struct
{
    float x;
    float y;
    float z;
} SensorOffsets_t;
SensorOffsets_t OFFSETS_MPU6050 = {150.0f, -940.0f, -1014.0f};
SensorOffsets_t OFFSETS_BMI160 = {850.0f, -610.0f, -374.0f};
SensorOffsets_t *current_offsets = NULL;

// --- Globais e Handles ---
static bool g_user_request_motor = false; // Intenção do usuário
static bool g_sensor_is_valid = false;    // Estado real do hardware
static uint8_t g_current_sensor_addr = 0;

adc_oneshot_unit_handle_t adc_handle;
SemaphoreHandle_t xI2CMutex; // Protege o acesso ao I2C

// ==========================================================
//    FUNÇÕES AUXILIARES I2C (Mantidas do original)
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
    for (uint8_t i = 1; i < 127; i++)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t res = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(20));
        i2c_cmd_link_delete(cmd);
        if (res == ESP_OK)
            return i;
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
//    TASK 1: GERENCIAMENTO DE HARDWARE (SENSOR WATCHDOG)
//    Responsabilidade: Detectar sensor e monitorar conexão
// ==========================================================
void task_sensor_monitor(void *pvParameters)
{
    while (1)
    {
        // Tenta pegar o Mutex para usar o I2C
        if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) == pdTRUE)
        {

            if (g_sensor_is_valid)
            {
                // Modo Monitoramento: Verifica se ainda está vivo
                if (!check_sensor_health(g_current_sensor_addr))
                {
                    ESP_LOGE(TAG, "WATCHDOG: Sensor perdeu conexao!");
                    g_sensor_is_valid = false;
                    g_current_sensor_addr = 0;
                    current_offsets = NULL;
                }
            }
            else
            {
                // Modo Busca: Tenta encontrar sensor
                uint8_t addr = i2c_scan_bus();
                if (addr != 0)
                {
                    ESP_LOGI(TAG, "Novo dispositivo detectado em 0x%02X", addr);

                    uint8_t who_am_i = 0;
                    uint8_t reg_check = 0x75; // Padrão MPU
                    i2c_master_write_read_device(I2C_MASTER_NUM, addr, &reg_check, 1, &who_am_i, 1, pdMS_TO_TICKS(100));

                    bool identified = false;

                    // Lógica de identificação (Simplificada do seu original)
                    if (who_am_i == 0x68 || who_am_i == 0x70)
                    { // MPU
                        if (MPU_Init() == ESP_OK)
                        {
                            current_offsets = &OFFSETS_MPU6050;
                            identified = true;
                            ESP_LOGI(TAG, "MPU6050 Inicializado.");
                        }
                    }
                    else
                    {
                        // Tenta BMI logic here if needed...
                        // Para brevidade, assumindo MPU ou checando BMI se MPU falhar
                        reg_check = 0x00;
                        i2c_master_write_read_device(I2C_MASTER_NUM, addr, &reg_check, 1, &who_am_i, 1, pdMS_TO_TICKS(100));
                        if (who_am_i == 0xD1)
                        {
                            if (BMI160_Init() == ESP_OK)
                            {
                                current_offsets = &OFFSETS_BMI160;
                                identified = true;
                                ESP_LOGI(TAG, "BMI160 Inicializado.");
                            }
                        }
                    }

                    if (identified)
                    {
                        g_current_sensor_addr = addr;
                        g_sensor_is_valid = true;
                    }
                }
            }
            // Solta o I2C para outras tasks usarem
            xSemaphoreGive(xI2CMutex);
        }

        // Se estiver conectado, checa a cada 500ms. Se estiver buscando, checa a cada 1s.
        vTaskDelay(pdMS_TO_TICKS(g_sensor_is_valid ? 500 : 1000));
    }
}

// ==========================================================
//    TASK 2: INTERFACE DO USUÁRIO & CONTROLE DO RELÉ
//    Responsabilidade: Botão, Segurança e Acionamento
// ==========================================================
void task_ui_control(void *pvParameters)
{
    uint32_t last_press = 0;
    bool last_relay_state = false;

    while (1)
    {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // 1. Leitura do Botão
        if (gpio_get_level(BOTAO_PIN) == 0)
        {
            if ((now - last_press) > DEBOUNCE_TIME_MS)
            {
                last_press = now;

                if (g_sensor_is_valid)
                {
                    g_user_request_motor = !g_user_request_motor;
                    ESP_LOGI(TAG, "Botao pressionado. Novo Estado Desejado: %d", g_user_request_motor);
                }
                else
                {
                    ESP_LOGW(TAG, "Acao negada: Sem sensor.");
                    // Feedback visual rápido de erro
                    for (int k = 0; k < 3; k++)
                    {
                        gpio_set_level(LED_SEM_FALHA, 1);
                        gpio_set_level(LED_COM_FALHA, 1);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        gpio_set_level(LED_SEM_FALHA, 0);
                        gpio_set_level(LED_COM_FALHA, 0);
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                }
            }
            // Espera soltar para não ficar alternando loucamente
            while (gpio_get_level(BOTAO_PIN) == 0)
                vTaskDelay(pdMS_TO_TICKS(20));
        }

        // 2. Lógica de Segurança e Atuação
        // O motor só liga se o usuário quer E o sensor está válido
        bool should_motor_be_on = g_user_request_motor && g_sensor_is_valid;

        // Se sensor caiu com motor ligado, desliga a solicitação do usuário por segurança
        if (g_user_request_motor && !g_sensor_is_valid)
        {
            g_user_request_motor = false;
            should_motor_be_on = false;
            ESP_LOGW(TAG, "SEGURANCA: Motor desligado por falha no sensor.");
        }

        // Aplica no Hardware apenas se mudou o estado
        if (should_motor_be_on != last_relay_state)
        {
            gpio_set_level(RELE_PIN, should_motor_be_on);
            last_relay_state = should_motor_be_on;

            if (!should_motor_be_on)
            {
                // Se desligou, apaga os LEDs de status
                gpio_set_level(LED_SEM_FALHA, 0);
                gpio_set_level(LED_COM_FALHA, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Loop rápido de UI
    }
}

// ==========================================================
//    TASK 3: PROCESSAMENTO DE DADOS & IA (TinyML)
//    Responsabilidade: Ler sensor, ADC, rodar inferência
// ==========================================================
void task_processing(void *pvParameters)
{
    int16_t ax, ay, az;
    uint32_t count_print = 0;
    float aux_x = 0, aux_y = 0, aux_z = 0;

    while (1)
    {
        // Só processa se o motor estiver efetivamente ligado e sensor OK
        if (g_sensor_is_valid && g_user_request_motor)
        {

            esp_err_t err = ESP_FAIL;

            // --- BLOQUEIO MUTEX PARA I2C ---
            if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) == pdTRUE)
            {
                if (current_offsets == &OFFSETS_MPU6050)
                {
                    err = MPU_ReadAccelerometer(&ax, &ay, &az);
                }
                else if (current_offsets == &OFFSETS_BMI160)
                {
                    err = BMI160_ReadAccelerometer(&ax, &ay, &az);
                }
                xSemaphoreGive(xI2CMutex);
            }
            // ------------------------------

            if (err == ESP_OK)
            {
                // Leitura ADC
                int adc_sum = 0;
                for (int i = 0; i < ADC_SAMPLES; i++)
                {
                    int r;
                    adc_oneshot_read(adc_handle, ADC_CHANNEL, &r);
                    adc_sum += r;
                }
                int adc_avg = adc_sum / ADC_SAMPLES;

                // Processamento IA
                float fx = (float)ax - current_offsets->x;
                float fy = (float)ay - current_offsets->y;
                float fz = (float)az - current_offsets->z;

                float input[4] = {fx, fy, fz, (float)adc_avg};
                // Normalização
                for (int i = 0; i < 4; i++)
                    input[i] = (input[i] - SCALER_MEAN[i]) / SCALER_SCALE[i];

                MotorFaultDetector_AddSample(input[0], input[1], input[2], input[3]);
                float conf[2];
                MotorFaultDetector_Predict(conf);

                uint32_t falha = (uint32_t)(conf[0] * 100.0f);

                // Atualização dos LEDs
                if (falha <= 70)
                {
                    gpio_set_level(LED_SEM_FALHA, 1);
                    gpio_set_level(LED_COM_FALHA, 0);
                }
                else
                {
                    gpio_set_level(LED_SEM_FALHA, 0);
                    gpio_set_level(LED_COM_FALHA, 1);
                }

                // Log (acumula médias para não spammar o terminal)
                aux_x += (float)ax;
                aux_y += (float)ay;
                aux_z += (float)az;
                count_print++;

                if (count_print >= 10)
                {
                    printf("Motor ON | Falha: %lu%% | Z Raw Med: %.0f | RPM: 700\n", falha, aux_z / 10.0f);
                    count_print = 0;
                    aux_x = 0;
                    aux_y = 0;
                    aux_z = 0;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50)); // Delay da amostragem (importante para o modelo)
        }
        else
        {
            // Se motor desligado, dorme mais tempo para economizar CPU
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

// ==========================================================
//    MAIN
// ==========================================================
void app_main(void)
{
    // 1. Inicializa I2C Hardware
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_LOGI(TAG, "I2C Init OK");

    // 2. Cria o Mutex
    xI2CMutex = xSemaphoreCreateMutex();

    // 3. Inicializa outros periféricos (GPIO, ADC, TinyML)
    ESP_ERROR_CHECK(MotorFaultDetector_Init());

    adc_oneshot_unit_init_cfg_t init_config = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    adc_oneshot_chan_cfg_t config = {.bitwidth = ADC_WIDTH, .atten = ADC_ATTEN};
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));

    gpio_reset_pin(RELE_PIN);
    gpio_set_direction(RELE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELE_PIN, 0);
    gpio_reset_pin(LED_COM_FALHA);
    gpio_set_direction(LED_COM_FALHA, GPIO_MODE_OUTPUT);
    gpio_reset_pin(LED_SEM_FALHA);
    gpio_set_direction(LED_SEM_FALHA, GPIO_MODE_OUTPUT);
    gpio_reset_pin(BOTAO_PIN);
    gpio_set_direction(BOTAO_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOTAO_PIN, GPIO_PULLUP_ONLY);

    // 4. Criação das Threads (Tasks)
    // Stack sizes (4096) são estimativas seguras. Ajuste se faltar memória.

    // Task 1: Interface (Prioridade Média)
    xTaskCreate(task_ui_control, "UI_Control", 4096, NULL, 5, NULL);

    // Task 2: Monitoramento Hardware (Prioridade Alta para I2C não travar)
    xTaskCreate(task_sensor_monitor, "Sensor_Mon", 4096, NULL, 6, NULL);

    // Task 3: Processamento IA (Prioridade Baixa/Média)
    xTaskCreate(task_processing, "AI_Process", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Sistema Multitarefa Iniciado!");
}