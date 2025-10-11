#ifndef MOTOR_TINYML_H
#define MOTOR_TINYML_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WINDOW_SIZE 1  // Tamanho da janela de dados para inferência

// Enumeração atualizada para os dois novos status
typedef enum {
    MOTOR_STATUS_FAULT = 0,      // Motor com defeito (correspondendo à saída 0 do modelo)
    MOTOR_STATUS_ON = 1          // Motor ligado e funcionando normal (correspondendo à saída 1)
} motor_status_t;

/**
 * @brief Inicializa o detector de falhas do motor
 * @return ESP_OK em sucesso, ESP_FAIL em falha
 */
esp_err_t MotorFaultDetector_Init(void);

/**
 * @brief Adiciona uma amostra (3 eixos de aceleração + corrente) ao buffer.
 * @param accel_x Aceleração no eixo X (em g)
 * @param accel_y Aceleração no eixo Y (em g)
 * @param accel_z Aceleração no eixo Z (em g)
 * @param current_adc Valor bruto lido do ADC para o sensor de corrente.
 */
void MotorFaultDetector_AddSample(float accel_x, float accel_y, float accel_z, float current_adc); // ATUALIZADO

/**
 * @brief Executa a predição de falha
 * @param confidence Array para armazenar as 2 probabilidades [FAULT, ON]
 * @return Status do motor (MOTOR_STATUS_FAULT ou MOTOR_STATUS_ON)
 */
motor_status_t MotorFaultDetector_Predict(float confidence[2]); // Atualizado para 2 posições

#ifdef __cplusplus
}
#endif

#endif // MOTOR_TINYML_H