/*
 * Cabeçalho principal da aplicação.
 *
 * Inclui bibliotecas comuns e define configurações globais.
 */
#ifndef MAIN_H
#define MAIN_H

// --- Includes Comuns da Aplicação ---
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mpu6500.h"

#define Delay_ms vTaskDelay
#define tempo pdMS_TO_TICKS

#endif // MAIN_H