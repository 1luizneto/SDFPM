#ifndef MOTOR_TINYML_H
#define MOTOR_TINYML_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WINDOW_SIZE 1  // Tamanho da janela de dados para inferência

typedef enum {
    MOTOR_STATUS_OFF = 0,        // Motor desligado
    MOTOR_STATUS_NORMAL = 1,     // Motor funcionando normal
    MOTOR_STATUS_FAULT = 2       // Motor com defeito
} motor_status_t;

/**
 * @brief Inicializa o detector de falhas do motor
 * @return ESP_OK em sucesso, ESP_FAIL em falha
 */
esp_err_t MotorFaultDetector_Init(void);

/**
 * @brief Adiciona uma amostra ao buffer de detecção
 * @param accel_x Aceleração no eixo X (em g)
 * @param accel_y Aceleração no eixo Y (em g)
 * @param accel_z Aceleração no eixo Z (em g)
 */
void MotorFaultDetector_AddSample(float accel_x, float accel_y, float accel_z);

/**
 * @brief Executa a predição de falha
 * @param confidence Array para armazenar as 3 probabilidades [OFF, NORMAL, FAULT]
 * @return Status do motor (MOTOR_STATUS_OFF, MOTOR_STATUS_NORMAL ou MOTOR_STATUS_FAULT)
 */
motor_status_t MotorFaultDetector_Predict(float confidence[3]);

#ifdef __cplusplus
}
#endif

#endif // MOTOR_TINYML_H