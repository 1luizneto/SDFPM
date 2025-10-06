#include "motor_tinyml.h"
#include "model_data_v1.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"  
#include "tensorflow/lite/schema/schema_generated.h"
#include <string.h>
#include <math.h> 
#include "esp_log.h"

static const char *TAG = "FAULT_DETECTOR";

// Buffer para o TensorFlow Lite (aumentado para suportar todas as ops)
constexpr int kTensorArenaSize = 40 * 1024; // 40KB
static uint8_t tensor_arena[kTensorArenaSize];

// Ponteiros do TFLite
static const tflite::Model *model = nullptr;
static tflite::MicroInterpreter *interpreter = nullptr;
static TfLiteTensor *input = nullptr;
static TfLiteTensor *output = nullptr;

// Buffer circular para armazenar amostras
static float sample_buffer[WINDOW_SIZE * 3]; // X, Y, Z
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
    model = tflite::GetModel(motor_v1_tflite);
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

void MotorFaultDetector_AddSample(float accel_x, float accel_y, float accel_z)
{
    // TESTE SEM NORMALIZAÇÃO - dados brutos
    sample_buffer[0] = accel_x;
    sample_buffer[1] = accel_y;
    sample_buffer[2] = accel_z;
    
    sample_count = 1;
    
    //ESP_LOGI(TAG, "Input bruto: [%.4f, %.4f, %.4f]", accel_x, accel_y, accel_z);
}

motor_status_t MotorFaultDetector_Predict(float confidence[3])
{
    // Inicializa com valores padrão
    confidence[0] = 0.0f;
    confidence[1] = 0.0f;
    confidence[2] = 0.0f;

    if (sample_count < 1)
    {
        //ESP_LOGW(TAG, "Nenhuma amostra disponível");
        return MOTOR_STATUS_NORMAL;
    }

    // Log dados de entrada
    //ESP_LOGI(TAG, "Input normalizado: [%.4f, %.4f, %.4f]",sample_buffer[0], sample_buffer[1], sample_buffer[2]);

    // Copia dados para o tensor de entrada
    if (input->type == kTfLiteInt8) {
        // Modelo quantizado - converter float para int8
        float scale = input->params.scale;
        int zero_point = input->params.zero_point;
        
        for (int i = 0; i < 3; i++) {
            int8_t quantized = (int8_t)((sample_buffer[i] / scale) + zero_point);
            input->data.int8[i] = quantized;
        }
        //ESP_LOGI(TAG, "Input quantizado para int8");
    } else {
        // Modelo float32
        for (int i = 0; i < 3; i++) {
            input->data.f[i] = sample_buffer[i];
        }
    }

    // Executa inferência
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk)
    {
        //ESP_LOGE(TAG, "Falha na inferência!");
        return MOTOR_STATUS_NORMAL;
    }

    // Lê resultados
    float prob_off = 0.0f;
    float prob_normal = 0.0f;
    float prob_fault = 0.0f;

    if (output->type == kTfLiteInt8) {
        // Modelo quantizado - converter int8 para float
        int8_t* output_int8 = output->data.int8;
        float scale = output->params.scale;
        int zero_point = output->params.zero_point;
        
        prob_off = (output_int8[0] - zero_point) * scale;
        prob_normal = (output_int8[1] - zero_point) * scale;
        prob_fault = (output_int8[2] - zero_point) * scale;
        
        //ESP_LOGI(TAG, "Output int8: [%d, %d, %d]", output_int8[0], output_int8[1], output_int8[2]);
    } else {
        prob_off = output->data.f[0];
        prob_normal = output->data.f[1];
        prob_fault = output->data.f[2];
    }

    // Aplicar softmax se necessário (normalizar probabilidades)
    float sum = expf(prob_off) + expf(prob_normal) + expf(prob_fault);
    prob_off = expf(prob_off) / sum;
    prob_normal = expf(prob_normal) / sum;
    prob_fault = expf(prob_fault) / sum;

    //ESP_LOGI(TAG, "Probabilidades - OFF: %.4f, NORMAL: %.4f, FAULT: %.4f", prob_off, prob_normal,prob_fault);

    confidence[0] = prob_off;
    confidence[1] = prob_normal;
    confidence[2] = prob_fault;

    // Retorna classe com maior probabilidade
    if (prob_off > prob_normal && prob_off > prob_fault) {
        return MOTOR_STATUS_OFF;
    } else if (prob_fault > prob_normal) {
        return MOTOR_STATUS_FAULT;
    } else {
        return MOTOR_STATUS_NORMAL;
    }
}