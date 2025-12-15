#include "motor_tinyml.h"
#include "model_data_v5.h" // MODELO 5 CATEGORIAS SEM OFFSET
#include "model_data_v11.h"
#include "model_data_v12.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"  
#include "tensorflow/lite/schema/schema_generated.h"
#include <string.h>
#include <math.h> 
#include "esp_log.h"

static const char *TAG = "FAULT_DETECTOR";

const float SCALER_MEAN[4] = {
    -14651.008128f,  // X
    -50.763613f,  // Y
    7589.043379f,  // Z
    16151.276905f,  // RPM
};

// Desvios padrão de cada feature calculadas durante o treinamento
const float SCALER_SCALE[4] = {
    190.850926f,  // X
    329.144817f,  // Y
    1131.521688f,  // Z
    8061.598235f,  // RPM
};

// Buffer para o TensorFlow Lite (aumentado para suportar todas as ops)
constexpr int kTensorArenaSize = 80 * 1024; // 80KB
static uint8_t tensor_arena[kTensorArenaSize];

// Ponteiros do TFLite
static const tflite::Model *model = nullptr;
static tflite::MicroInterpreter *interpreter = nullptr;
static TfLiteTensor *input = nullptr;
static TfLiteTensor *output = nullptr;

// Buffer circular para armazenar amostras
static float sample_buffer[ 4]; // X, Y, Z, corrente
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
    model = tflite::GetModel(motor_v12_tflite);
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

void MotorFaultDetector_AddSample(float accel_x, float accel_y, float accel_z, float rpm)
{
    // Armazena os 4 valores no buffer
    sample_buffer[0] = accel_x;
    sample_buffer[1] = accel_y;
    sample_buffer[2] = accel_z;
    sample_buffer[3] = rpm; // Novo valor de rpm

    sample_count = 1;
}

motor_class_t MotorFaultDetector_Predict(float *confidence_array)
{
    // [ERRO ANTERIOR]: O sample_count < 1 sugeria buffer de tempo. 
    // Para o modelo novo, basta ter 1 leitura válida.

    // 1. NORMALIZAÇÃO (Essencial!)
    // Variável temporária para guardar dados normalizados
    float input_data_normalized[4];
    
    // Supondo que sample_buffer tenha {X, Y, Z, RPM} brutos
    for(int i=0; i<4; i++) {
        input_data_normalized[i] = (sample_buffer[i] - SCALER_MEAN[i]) / SCALER_SCALE[i];
    }

    // 2. PREENCHIMENTO DO TENSOR (Com dados normalizados)
    if (input->type == kTfLiteInt8) {
        float scale = input->params.scale;
        int zero_point = input->params.zero_point;
        for (int i = 0; i < 4; i++) {
            // Quantiza o dado JÁ normalizado
            input->data.int8[i] = (int8_t)((input_data_normalized[i] / scale) + zero_point);
        }
    } else {
        for (int i = 0; i < 4; i++) {
            input->data.f[i] = input_data_normalized[i];
        }
    }

    // 3. INFERÊNCIA
    if (interpreter->Invoke() != kTfLiteOk) {
        ESP_LOGE(TAG, "Falha na inferência!");
        return CLASS_DESLIGADO;
    }

    // 4. PROCESSAMENTO DA SAÍDA (Correção do Double Softmax)
    float max_score = -1.0f; // Probabilidade nunca é negativa
    int max_index = 0;

    for (int i = 0; i < NUM_CLASSES; i++) {
        float val;
        
        // Se a saída for INT8, desquantiza para Float
        if (output->type == kTfLiteInt8) {
            float scale = output->params.scale;
            int zero_point = output->params.zero_point;
            val = (output->data.int8[i] - zero_point) * scale;
        } else {
            // Se for Float (seu caso atual), pega direto
            val = output->data.f[i];
        }

        // --- CORREÇÃO AQUI ---
        // NÃO aplicamos expf() nem dividimos pela soma.
        // O modelo TFLite já entrega a probabilidade pronta (Softmax já rodou na rede).
        
        confidence_array[i] = val; // O valor JÁ É a % (ex: 0.99)

        if (confidence_array[i] > max_score) {
            max_score = confidence_array[i];
            max_index = i;
        }
    }

    return (motor_class_t)max_index;
}