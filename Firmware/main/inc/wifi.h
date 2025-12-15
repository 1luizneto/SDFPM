#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// --- CONFIGURAÇÕES DE REDE ---
#define WIFI_SSID "Assert"
#define WIFI_PASS "w1f1:Assert!!"

// IP do seu computador e a rota da API
#define WEB_SERVER_URL "http://192.168.10.179:8000/api/leituras/registrar/"

// ID do Hardware fixo
#define HARDWARE_UID "ESP32-S3-002"

// --- Protótipos das Funções ---

/**
 * @brief Inicializa a conexão Wi-Fi em modo Station
 */
void wifi_app_init(void);

/**
 * @brief Verifica se o ESP32 está conectado e com IP atribuído
 * @return true se conectado, false caso contrário
 */
bool wifi_app_check_connection(void);

/**
 * @brief Envia o pacote JSON de telemetria via HTTP POST
 * * @param x Valor bruto do eixo X
 * @param y Valor bruto do eixo Y
 * @param z Valor bruto do eixo Z
 * @param rpm Valor calculado de RPM
 * @param status Valor de 0 a 4 indicando o diagnóstico (0=Normal, etc)
 * @return esp_err_t ESP_OK se enviou (200 OK), erro caso contrário
 */
esp_err_t wifi_send_telemetry(int16_t x, int16_t y, int16_t z, float rpm, int status);

#endif