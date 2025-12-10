#ifndef MOTOR_TINYML_H
#define MOTOR_TINYML_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ATENÇÃO: A ordem aqui deve ser A MESMA do seu treinamento (labels.txt)
typedef enum {
    CLASS_DESLIGADO = 0,
    CLASS_FALHA_1,   // ex: Bloqueio
    CLASS_FALHA_2,   // ex: Desbalanceamento
    CLASS_FALHA_3,   // ex: Rolamento
    CLASS_NORMAL     // ex: Ligado Normal
} motor_class_t;

extern const float SCALER_MEAN[4];
extern const float SCALER_SCALE[4];

#define NUM_CLASSES 5

esp_err_t MotorFaultDetector_Init(void);
void MotorFaultDetector_AddSample(float accel_x, float accel_y, float accel_z, float rpm);

// Agora retorna o índice da classe vencedora e preenche o array de confianças
motor_class_t MotorFaultDetector_Predict(float *confidence_array);

#ifdef __cplusplus
}
#endif

#endif