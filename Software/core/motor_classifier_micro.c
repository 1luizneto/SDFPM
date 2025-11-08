/**
 * Motor Classifier - Implementação para Microcontrolador
 *
 * Este arquivo contém a implementação da normalização de dados para o modelo
 * Motor Classifier v2 treinado com TensorFlow/Keras.
 *
 * Características do modelo:
 * - Entrada: 4 features (X, Y, Z, ADC_RAW)
 * - Saída: 2 classes (defeito=0, ligado=1)
 * - Normalização: StandardScaler
 *
 * Autor: NexusML API
 * Data: 2025-10-10
 * Versão: 1.0
 */

#include <stdio.h>
#include <math.h>
#include <stdint.h>

// ============================================================================
// PARÂMETROS DO STANDARDSCALER (extraídos do modelo treinado)
// ============================================================================

// Médias de cada feature durante o treinamento
const float SCALER_MEAN[4] = {
    -10138.618036f,  // X (acelerômetro eixo X)
    -407.879632f,    // Y (acelerômetro eixo Y)
    11927.490799f,   // Z (acelerômetro eixo Z)
    3027.813713f     // ADC_RAW (valor bruto do ADC)
};

// Desvios padrão de cada feature durante o treinamento
const float SCALER_SCALE[4] = {
    1747.075269f,    // X (acelerômetro eixo X)
    786.377335f,     // Y (acelerômetro eixo Y)
    2424.841654f,    // Z (acelerômetro eixo Z)
    53.533303f       // ADC_RAW (valor bruto do ADC)
};

// ============================================================================
// DEFINIÇÕES E CONSTANTES
// ============================================================================

#define NUM_FEATURES 4
#define NUM_CLASSES 2

// Códigos de classe
typedef enum {
    MOTOR_DEFEITO = 0,
    MOTOR_LIGADO = 1
} motor_class_t;

// Estrutura para dados do sensor
typedef struct {
    float x;         // Acelerômetro X
    float y;         // Acelerômetro Y
    float z;         // Acelerômetro Z
    float adc_raw;   // Valor bruto do ADC
} sensor_data_t;

// Estrutura para dados normalizados
typedef struct {
    float features[NUM_FEATURES];
} normalized_data_t;

// Estrutura para resultado da predição
typedef struct {
    motor_class_t predicted_class;
    float probabilities[NUM_CLASSES];
    float confidence;
} prediction_result_t;

// ============================================================================
// FUNÇÕES DE NORMALIZAÇÃO
// ============================================================================

/**
 * Normaliza os dados do sensor usando StandardScaler
 * Fórmula: (valor - média) / desvio_padrão
 *
 * @param sensor_data: Dados brutos do sensor
 * @param normalized: Estrutura de saída com dados normalizados
 */
void normalize_sensor_data(const sensor_data_t* sensor_data, normalized_data_t* normalized) {
    normalized->features[0] = (sensor_data->x - SCALER_MEAN[0]) / SCALER_SCALE[0];
    normalized->features[1] = (sensor_data->y - SCALER_MEAN[1]) / SCALER_SCALE[1];
    normalized->features[2] = (sensor_data->z - SCALER_MEAN[2]) / SCALER_SCALE[2];
    normalized->features[3] = (sensor_data->adc_raw - SCALER_MEAN[3]) / SCALER_SCALE[3];
}

/**
 * Versão otimizada para normalização in-place
 * Modifica diretamente o array de entrada
 *
 * @param data: Array com os 4 valores [x, y, z, adc_raw]
 */
void normalize_data_inplace(float data[NUM_FEATURES]) {
    for (int i = 0; i < NUM_FEATURES; i++) {
        data[i] = (data[i] - SCALER_MEAN[i]) / SCALER_SCALE[i];
    }
}

/**
 * Normaliza uma única amostra de dados
 *
 * @param x: Acelerômetro X
 * @param y: Acelerômetro Y
 * @param z: Acelerômetro Z
 * @param adc_raw: Valor bruto do ADC
 * @param output: Array de saída com 4 valores normalizados
 */
void normalize_single_sample(float x, float y, float z, float adc_raw, float output[NUM_FEATURES]) {
    output[0] = (x - SCALER_MEAN[0]) / SCALER_SCALE[0];
    output[1] = (y - SCALER_MEAN[1]) / SCALER_SCALE[1];
    output[2] = (z - SCALER_MEAN[2]) / SCALER_SCALE[2];
    output[3] = (adc_raw - SCALER_MEAN[3]) / SCALER_SCALE[3];
}

// ============================================================================
// FUNÇÕES AUXILIARES PARA INTERPRETAÇÃO
// ============================================================================

/**
 * Aplica softmax para converter logits em probabilidades
 * (Use esta função se o modelo retornar logits em vez de probabilidades)
 *
 * @param logits: Array de entrada com logits
 * @param probabilities: Array de saída com probabilidades
 * @param size: Número de elementos
 */
void softmax(const float* logits, float* probabilities, int size) {
    float max_logit = logits[0];

    // Encontrar o máximo para estabilidade numérica
    for (int i = 1; i < size; i++) {
        if (logits[i] > max_logit) {
            max_logit = logits[i];
        }
    }

    // Calcular exponenciais e soma
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        probabilities[i] = expf(logits[i] - max_logit);
        sum += probabilities[i];
    }

    // Normalizar
    for (int i = 0; i < size; i++) {
        probabilities[i] /= sum;
    }
}

/**
 * Encontra a classe com maior probabilidade
 *
 * @param probabilities: Array com probabilidades das classes
 * @param size: Número de classes
 * @return: Índice da classe predita
 */
int argmax(const float* probabilities, int size) {
    int max_index = 0;
    float max_value = probabilities[0];

    for (int i = 1; i < size; i++) {
        if (probabilities[i] > max_value) {
            max_value = probabilities[i];
            max_index = i;
        }
    }

    return max_index;
}

/**
 * Interpreta o resultado da predição
 *
 * @param probabilities: Probabilidades das classes [defeito, ligado]
 * @param result: Estrutura de saída com resultado interpretado
 */
void interpret_prediction(const float probabilities[NUM_CLASSES], prediction_result_t* result) {
    // Encontrar classe predita
    int predicted_index = argmax(probabilities, NUM_CLASSES);
    result->predicted_class = (motor_class_t)predicted_index;

    // Copiar probabilidades
    result->probabilities[0] = probabilities[0];  // defeito
    result->probabilities[1] = probabilities[1];  // ligado

    // Calcular confiança (maior probabilidade)
    result->confidence = probabilities[predicted_index];
}

// ============================================================================
// FUNÇÕES DE EXEMPLO E TESTE
// ============================================================================

/**
 * Função de exemplo para simular uma predição completa
 * (Esta função seria substituída pela chamada do modelo TFLite)
 *
 * @param normalized_data: Dados normalizados de entrada
 * @param result: Resultado da predição
 */
void predict_motor_status(const normalized_data_t* normalized_data, prediction_result_t* result) {
    // EXEMPLO: Esta seria a chamada para o modelo TFLite
    // Aqui simulamos com regras simples baseadas nos dados de treinamento

    float x_norm = normalized_data->features[0];
    float y_norm = normalized_data->features[1];
    float z_norm = normalized_data->features[2];
    float adc_norm = normalized_data->features[3];

    // Simulação baseada em características observadas nos dados reais
    float probabilities[NUM_CLASSES];

    // Regra simples: se variação for muito alta, provavelmente é defeito
    float variation = fabsf(x_norm) + fabsf(y_norm) + fabsf(z_norm);

    if (variation > 2.0f || adc_norm > 1.0f) {
        probabilities[0] = 0.8f;  // defeito
        probabilities[1] = 0.2f;  // ligado
    } else {
        probabilities[0] = 0.1f;  // defeito
        probabilities[1] = 0.9f;  // ligado
    }

    interpret_prediction(probabilities, result);
}

/**
 * Função de teste com dados reais
 */
void test_with_real_data() {
    printf("🧪 TESTE COM DADOS REAIS DO MOTOR\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");

    // Dados reais de motor ligado (funcionamento normal)
    sensor_data_t motor_ligado[] = {
        {-9810, -618, 12151, 2955},
        {-9696, -491, 12357, 2952},
        {-9863, -438, 12193, 2949}
    };

    // Dados reais de motor com defeito
    sensor_data_t motor_defeito[] = {
        {-12026, -1683, 9706, 3057},
        {-9557, 301, 9927, 3083},
        {-10784, 172, 15377, 3074}
    };

    printf("\n📊 TESTANDO MOTOR LIGADO (NORMAL):\n");
    printf("-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "\n");

    for (int i = 0; i < 3; i++) {
        normalized_data_t normalized;
        prediction_result_t result;

        // Normalizar dados
        normalize_sensor_data(&motor_ligado[i], &normalized);

        // Fazer predição (simulada)
        predict_motor_status(&normalized, &result);

        printf("Amostra %d:\n", i + 1);
        printf("  Dados: X=%.0f, Y=%.0f, Z=%.0f, ADC=%.0f\n",
               motor_ligado[i].x, motor_ligado[i].y,
               motor_ligado[i].z, motor_ligado[i].adc_raw);
        printf("  Normalizados: X=%.3f, Y=%.3f, Z=%.3f, ADC=%.3f\n",
               normalized.features[0], normalized.features[1],
               normalized.features[2], normalized.features[3]);
        printf("  Predição: %s\n",
               result.predicted_class == MOTOR_DEFEITO ? "DEFEITO" : "LIGADO");
        printf("  Confiança: %.1f%%\n", result.confidence * 100);
        printf("\n");
    }

    printf("\n📊 TESTANDO MOTOR COM DEFEITO:\n");
    printf("-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "\n");

    for (int i = 0; i < 3; i++) {
        normalized_data_t normalized;
        prediction_result_t result;

        // Normalizar dados
        normalize_sensor_data(&motor_defeito[i], &normalized);

        // Fazer predição (simulada)
        predict_motor_status(&normalized, &result);

        printf("Amostra %d:\n", i + 1);
        printf("  Dados: X=%.0f, Y=%.0f, Z=%.0f, ADC=%.0f\n",
               motor_defeito[i].x, motor_defeito[i].y,
               motor_defeito[i].z, motor_defeito[i].adc_raw);
        printf("  Normalizados: X=%.3f, Y=%.3f, Z=%.3f, ADC=%.3f\n",
               normalized.features[0], normalized.features[1],
               normalized.features[2], normalized.features[3]);
        printf("  Predição: %s\n",
               result.predicted_class == MOTOR_DEFEITO ? "DEFEITO" : "LIGADO");
        printf("  Confiança: %.1f%%\n", result.confidence * 100);
        printf("\n");
    }
}

/**
 * Exemplo de uso no loop principal do microcontrolador
 */
void microcontroller_main_loop_example() {
    printf("\n🔄 EXEMPLO DE USO NO MICROCONTROLADOR:\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");

    // Simular leitura de sensores (substitua pelas suas funções)
    float sensor_x = -9810.0f;      // Leia do acelerômetro
    float sensor_y = -618.0f;       // Leia do acelerômetro
    float sensor_z = 12151.0f;      // Leia do acelerômetro
    float adc_value = 2955.0f;      // Leia do ADC

    // Normalizar dados
    float normalized[NUM_FEATURES];
    normalize_single_sample(sensor_x, sensor_y, sensor_z, adc_value, normalized);

    printf("Dados brutos: X=%.0f, Y=%.0f, Z=%.0f, ADC=%.0f\n",
           sensor_x, sensor_y, sensor_z, adc_value);
    printf("Dados normalizados: [%.3f, %.3f, %.3f, %.3f]\n",
           normalized[0], normalized[1], normalized[2], normalized[3]);

    // Aqui você chamaria o modelo TFLite:
    // interpreter.SetInputTensor(0, normalized);
    // interpreter.Invoke();
    // float* output = interpreter.GetOutputTensor(0);

    printf("\n💡 Próximos passos:\n");
    printf("1. Integre este código com TensorFlow Lite Micro\n");
    printf("2. Carregue o modelo Motor_Classifier_v2.tflite\n");
    printf("3. Substitua predict_motor_status() pela chamada do modelo real\n");
    printf("4. Implemente as funções de leitura dos sensores\n");
}

// ============================================================================
// FUNÇÃO PRINCIPAL (PARA TESTE)
// ============================================================================

int main() {
    printf("🚀 MOTOR CLASSIFIER - IMPLEMENTAÇÃO PARA MICROCONTROLADOR\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");
    printf("Versão: 1.0\n");
    printf("Modelo: Motor_Classifier_v2\n");
    printf("Features: X, Y, Z, ADC_RAW\n");
    printf("Classes: defeito (0), ligado (1)\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");

    // Executar testes
    test_with_real_data();
    microcontroller_main_loop_example();

    printf("\n✅ TESTE CONCLUÍDO!\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "\n");

    return 0;
}

// ============================================================================
// NOTAS PARA IMPLEMENTAÇÃO NO MICROCONTROLADOR
// ============================================================================

/*
INSTRUÇÕES PARA USO NO MICROCONTROLADOR:

1. PREPARAÇÃO:
   - Copie este arquivo para seu projeto
   - Remova ou comente a função main() se não precisar dos testes
   - Adapte as includes conforme seu ambiente

2. INTEGRAÇÃO COM TFLITE MICRO:
   - Inclua as bibliotecas do TensorFlow Lite Micro
   - Carregue o modelo Motor_Classifier_v2.tflite
   - Substitua a função predict_motor_status() pela chamada real do modelo

3. LEITURA DE SENSORES:
   - Implemente funções para ler o acelerômetro (X, Y, Z)
   - Implemente função para ler o ADC
   - Chame normalize_single_sample() antes de enviar para o modelo

4. EXEMPLO DE USO:
   ```c
   // Ler sensores
   float x = read_accelerometer_x();
   float y = read_accelerometer_y();
   float z = read_accelerometer_z();
   float adc = read_adc_value();

   // Normalizar
   float normalized[4];
   normalize_single_sample(x, y, z, adc, normalized);

   // Predizer com TFLite
   interpreter.SetInputTensor(0, normalized);
   interpreter.Invoke();
   float* output = interpreter.GetOutputTensor(0);

   // Interpretar resultado
   prediction_result_t result;
   interpret_prediction(output, &result);

   if (result.predicted_class == MOTOR_DEFEITO) {
       // Motor com defeito detectado!
       // Acionar alarme, LED, etc.
   }
   ```

5. OTIMIZAÇÕES:
   - Use normalize_data_inplace() para economizar memória
   - Considere usar fixed-point arithmetic em vez de float se necessário
   - Ajuste o tamanho dos buffers conforme disponibilidade de RAM

6. VALIDAÇÃO:
   - Compare os resultados com o modelo Python
   - Teste com dados conhecidos (motor ligado vs defeito)
   - Monitore o consumo de memória e CPU
*/
