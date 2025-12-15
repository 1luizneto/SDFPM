#include "wifi.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"

static const char *TAG = "WIFI_APP";

// Event Group para gerenciar estados da conexão
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static int s_retry_num = 0;

// ==========================================================
//    HANDLERS DE EVENTOS WI-FI
// ==========================================================
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
        s_retry_num++;
        ESP_LOGI(TAG, "Tentando reconectar ao AP... (Tentativa %d)", s_retry_num);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Conectado! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// ==========================================================
//    INICIALIZAÇÃO
// ==========================================================
void wifi_app_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi Inicializado. SSID: %s", WIFI_SSID);
}

// ==========================================================
//    VERIFICAÇÃO DE STATUS
// ==========================================================
bool wifi_app_check_connection(void)
{
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT);
}

// ==========================================================
//    ENVIO DE DADOS (HTTP POST)
// ==========================================================
esp_err_t wifi_send_telemetry(int16_t x, int16_t y, int16_t z, float rpm, int status)
{
    if (!wifi_app_check_connection())
    {
        ESP_LOGE(TAG, "Falha no envio: Wi-Fi desconectado");
        return ESP_FAIL;
    }

    esp_http_client_config_t config = {
        .url = WEB_SERVER_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Falha ao iniciar cliente HTTP");
        return ESP_FAIL;
    }

    // Buffer para o JSON
    char json_payload[150];

    /* Novo Formato do JSON:
    {
        "uid_hardware": "ESP32-S3-001",
        "eixo_x": 15400,
        "eixo_y": 1300,
        "eixo_z": 1500,
        "rpm": 3500.50,
        "status": 0  <-- Inteiro de 0 a 4
    }
    */
    snprintf(json_payload, sizeof(json_payload),
             "{\"uid_hardware\":\"%s\",\"eixo_x\":%d,\"eixo_y\":%d,\"eixo_z\":%d,\"rpm\":%.2f,\"em_falha\":%d}",
             HARDWARE_UID,
             x,
             y,
             z,
             rpm,
             status);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code >= 200 && status_code < 300)
        {
            // Sucesso
        }
        else
        {
            ESP_LOGW(TAG, "Servidor HTTP retornou erro: %d", status_code);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Erro na requisicao HTTP: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}