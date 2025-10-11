#include "motor_tinyml.h"
#include "model_data_v1.h"
#include "model_data_v2.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"  
#include "tensorflow/lite/schema/schema_generated.h"
#include <string.h>
#include <math.h> 
#include "esp_log.h"

static const char *TAG = "FAULT_DETECTOR";

// Buffer para o TensorFlow Lite (aumentado para suportar todas as ops)
constexpr int kTensorArenaSize = 80 * 1024; // 80KB
static uint8_t tensor_arena[kTensorArenaSize];

// Ponteiros do TFLite
static const tflite::Model *model = nullptr;
static tflite::MicroInterpreter *interpreter = nullptr;
static TfLiteTensor *input = nullptr;
static TfLiteTensor *output = nullptr;

// Buffer circular para armazenar amostras
static float sample_buffer[WINDOW_SIZE * 4]; // X, Y, Z, corrente
static int sample_count = 0;

// static float mean_x = 0.0f;
// static float mean_y = 0.0f;
// static float mean_z = 0.0f;
// static float std_x = 1.0f;
// static float std_y = 1.0f;
// static float std_z = 1.0f;

esp_err_t MotorFaultDetector_Init(void)
{
    // 1. Carrega o modelo
    model = tflite::GetModel(motor_v2_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        ESP_LOGE(TAG, "Versão do modelo incompatível!");
        return ESP_FAIL;
    }

    // 2. Configura TODAS as operações necessárias
    static tflite::MicroMutableOpResolver<30> resolver;
    
    // Operações básicas de NN
    resolver.AddFullyConnected();
    resolver.AddSoftmax();
    resolver.AddRelu();
    resolver.AddRelu6();
    resolver.AddLogistic();
    resolver.AddTanh();
    
    // Operações de quantização
    resolver.AddQuantize();
    resolver.AddDequantize();
    
    // Operações de reshape/shape
    resolver.AddShape();
    resolver.AddReshape();
    resolver.AddExpandDims();
    resolver.AddSqueeze();
    resolver.AddStridedSlice();
    resolver.AddSlice();
    
    // Operações de agregação
    resolver.AddPack();
    resolver.AddUnpack();
    resolver.AddConcatenation();
    resolver.AddMean();
    resolver.AddSum();
    resolver.AddMaximum();
    resolver.AddMinimum();
    
    // Operações de convolução
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddMaxPool2D();
    resolver.AddAveragePool2D();
    
    // Operações matemáticas
    resolver.AddAdd();
    resolver.AddMul();
    resolver.AddSub();
    resolver.AddDiv();

    // 3. Cria o interpretador
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    // 4. Aloca tensores
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        ESP_LOGE(TAG, "Falha ao alocar tensores!");
        ESP_LOGE(TAG, "Arena size: %d bytes", kTensorArenaSize);
        return ESP_FAIL;
    }

    // 5. Obtém ponteiros para input/output
    input = interpreter->input(0);
    output = interpreter->output(0);

    ESP_LOGI(TAG, "Modelo TinyML inicializado com sucesso!");
    ESP_LOGI(TAG, "Input shape: [%d, %d]", input->dims->data[0], input->dims->data[1]);
    ESP_LOGI(TAG, "Output shape: [%d, %d]", output->dims->data[0], output->dims->data[1]);

    return ESP_OK;
}

void MotorFaultDetector_AddSample(float accel_x, float accel_y, float accel_z, float current_adc)
{
    // Armazena os 4 valores no buffer
    sample_buffer[0] = accel_x;
    sample_buffer[1] = accel_y;
    sample_buffer[2] = accel_z;
    sample_buffer[3] = current_adc; // Novo valor de corrente

    sample_count = 1;
}

motor_status_t MotorFaultDetector_Predict(float confidence[2])
{
    // Inicializa com valores padrão
    confidence[0] = 0.0f; // Probabilidade de Falha
    confidence[1] = 0.0f; // Probabilidade de Ligado/Normal

    if (sample_count < 1)
    {
        return MOTOR_STATUS_FAULT; // Retorna um padrão seguro se não houver amostras
    }

    // Copia dados para o tensor de entrada (lógica inalterada)
    if (input->type == kTfLiteInt8) {
        float scale = input->params.scale;
        int zero_point = input->params.zero_point;
        for (int i = 0; i < 4; i++) {
            input->data.int8[i] = (int8_t)((sample_buffer[i] / scale) + zero_point);
        }
    } else {
        for (int i = 0; i < 4; i++) {
            input->data.f[i] = sample_buffer[i];
        }
    }

    // Executa inferência
    if (interpreter->Invoke() != kTfLiteOk)
    {
        ESP_LOGE(TAG, "Falha na inferência!");
        return MOTOR_STATUS_FAULT; // Retorna um padrão seguro em caso de erro
    }

    // Lê os resultados do modelo (agora com 2 saídas)
    float prob_fault = 0.0f;
    float prob_on = 0.0f;

    if (output->type == kTfLiteInt8) {
        // Modelo quantizado
        float scale = output->params.scale;
        int zero_point = output->params.zero_point;
        
        // Saída 0: Defeito
        prob_fault = (output->data.int8[0] - zero_point) * scale;
        // Saída 1: Ligado
        prob_on = (output->data.int8[1] - zero_point) * scale;

    } else {
        // Modelo float32
        // Saída 0: Defeito
        prob_fault = output->data.f[0];
        // Saída 1: Ligado
        prob_on = output->data.f[1];
    }

    // Aplicar softmax para normalizar as probabilidades (garantir que somem 1)
    // Se o seu modelo já tiver uma camada Softmax no final, esta etapa pode ser removida.
    // Mas é seguro mantê-la se os valores de saída forem "logits" brutos.
    float sum = expf(prob_fault) + expf(prob_on);
    prob_fault = expf(prob_fault) / sum;
    prob_on = expf(prob_on) / sum;

    // Armazena as probabilidades no array de saída
    confidence[0] = prob_fault;
    confidence[1] = prob_on;

    //ESP_LOGI(TAG, "Probabilidades - FALHA: %.4f, LIGADO: %.4f", prob_fault, prob_on);

    // Retorna a classe com maior probabilidade
    if (prob_fault > prob_on) {
        return MOTOR_STATUS_FAULT;
    } else {
        return MOTOR_STATUS_ON;
    }
}