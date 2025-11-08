"""
Script de Teste para o Modelo Motor Classifier
Testa o modelo treinado com dados simulados e reais
"""

import numpy as np
import tensorflow as tf
from tensorflow import keras
import joblib
import requests
import os
from sklearn.preprocessing import LabelEncoder
import json

# Configurações do modelo
MODEL_ID = "bef3f1ec-f8ac-4d9b-ad5f-118c319d9b1d"
BASE_URL = "http://127.0.0.1:8000"
NAME_MODEL = "Motor_Classifier_v2"
MODELS_DIR = "test_models"

# Criar diretório para modelos se não existir
os.makedirs(MODELS_DIR, exist_ok=True)

def download_model_files():
    """Download dos arquivos do modelo da API"""
    print("📥 Baixando arquivos do modelo...")

    files_to_download = {
        'model': f"{BASE_URL}/media/models/{MODEL_ID}/{NAME_MODEL}.h5",
        'scaler': f"{BASE_URL}/media/models/{MODEL_ID}/{NAME_MODEL}_scaler.pkl",
        'tflite': f"{BASE_URL}/media/models/{MODEL_ID}/{NAME_MODEL}.tflite",
    }

    local_files = {}

    for file_type, url in files_to_download.items():
        try:
            response = requests.get(url)
            if response.status_code == 200:
                # Salvar arquivo localmente
                if file_type == 'model':
                    filepath = os.path.join(MODELS_DIR, 'model.h5')
                elif file_type == 'scaler':
                    filepath = os.path.join(MODELS_DIR, 'scaler.pkl')
                else:
                    filepath = os.path.join(MODELS_DIR, 'model.tflite')

                with open(filepath, 'wb') as f:
                    f.write(response.content)

                local_files[file_type] = filepath
                print(f"✅ {file_type} baixado: {filepath}")
            else:
                print(f"❌ Erro ao baixar {file_type}: Status {response.status_code}")
        except Exception as e:
            print(f"❌ Erro ao baixar {file_type}: {e}")

    return local_files

def load_keras_model(model_path, scaler_path):
    """Carrega o modelo Keras e o scaler"""
    print("\n🔧 Carregando modelo Keras...")

    # Carregar modelo
    model = keras.models.load_model(model_path)
    print(f"✅ Modelo carregado: {model_path}")

    # Carregar scaler
    scaler = joblib.load(scaler_path)
    print(f"✅ Scaler carregado: {scaler_path}")

    return model, scaler

def load_tflite_model(tflite_path):
    """Carrega o modelo TFLite"""
    print("\n🔧 Carregando modelo TFLite...")

    interpreter = tf.lite.Interpreter(model_path=tflite_path)
    interpreter.allocate_tensors()
    print(f"✅ Modelo TFLite carregado: {tflite_path}")

    return interpreter

def generate_test_data():
    """Gera dados de teste baseados em dados reais"""
    print("\n📊 Gerando dados de teste baseados em dados reais...")

    # DADOS REAIS DO MOTOR LIGADO (funcionamento normal)
    # Características: X ~ -9700 a -9900, Y ~ -400 a -650, Z ~ 12100 a 12350, ADC ~ 2950 a 2965
    motor_ligado = [
        [-9810, -618, 12151, 2955],   # Dado real
        [-9696, -491, 12357, 2952],   # Dado real
        [-9863, -438, 12193, 2949],   # Dado real
        [-9785, -640, 12183, 2960],   # Dado real
        [-9736, -497, 12295, 2961],   # Dado real
        [-9872, -392, 12153, 2959],   # Dado real
        [-9821, -637, 12155, 2962],   # Dado real
        [-9716, -444, 12271, 2956],   # Dado real
        [-9834, -456, 12112, 2950],   # Dado real
        [-9812, -620, 12194, 2954],   # Dado real
    ]

    # DADOS REAIS DO MOTOR COM DEFEITO (vibração irregular)
    # Características: X varia muito (-8200 a -13100), Y varia (-1870 a +1100), Z varia (8100 a 15400), ADC ~ 3060 a 3090
    motor_defeito = [
        [-12026, -1683, 9706, 3057],   # Dado real
        [-9557, 301, 9927, 3083],      # Dado real
        [-10784, 172, 15377, 3074],    # Dado real
        [-11443, -1587, 9076, 3075],   # Dado real
        [-9538, 495, 10443, 3089],     # Dado real
        [-11251, -168, 15412, 3075],   # Dado real
        [-10854, -1390, 8644, 3073],   # Dado real
        [-9553, 594, 11207, 3090],     # Dado real
        [-11713, -426, 15156, 3072],   # Dado real
        [-10018, -1149, 8365, 3074],   # Dado real
    ]

    # Dados intermediários para teste (mistura características)
    dados_ambiguos = [
        [-10200, -800, 11000, 3010],   # Entre normal e defeito
        [-9900, -100, 11500, 3020],    # Ligeiramente anormal
        [-10500, -900, 10800, 3000],   # Zona cinzenta
    ]

    test_samples = {
        'ligado_normal': np.array(motor_ligado, dtype=np.float32),
        'defeito_real': np.array(motor_defeito, dtype=np.float32),
        'ambiguo': np.array(dados_ambiguos, dtype=np.float32)
    }

    return test_samples

def analyze_data_characteristics(samples):
    """Analisa as características estatísticas dos dados"""
    print("\n📊 ANÁLISE ESTATÍSTICA DOS DADOS:")
    print("=" * 60)

    for category, data in samples.items():
        print(f"\n{category.upper()}:")
        print("-" * 40)

        # Calcular estatísticas por coluna
        features = ['X', 'Y', 'Z', 'ADC_RAW']
        for i, feature in enumerate(features):
            values = data[:, i]
            print(f"  {feature}:")
            print(f"    Média: {np.mean(values):.1f}")
            print(f"    Std Dev: {np.std(values):.1f}")
            print(f"    Min: {np.min(values):.1f}")
            print(f"    Max: {np.max(values):.1f}")
            print(f"    Range: {np.max(values) - np.min(values):.1f}")

def predict_keras(model, scaler, samples):
    """Faz predições usando o modelo Keras"""
    print("\n🔮 PREDIÇÕES COM MODELO KERAS:")
    print("=" * 60)

    classes = ['defeito', 'ligado']  # Ordem alfabética (como o LabelEncoder organiza)

    for category, data in samples.items():
        print(f"\n📌 Testando dados de '{category}':")
        print("-" * 50)

        # Normalizar dados
        data_scaled = scaler.transform(data)

        # Fazer predições
        predictions = model.predict(data_scaled, verbose=0)
        predicted_classes = np.argmax(predictions, axis=1)

        # Estatísticas das predições
        defeito_count = sum(predicted_classes == 0)
        ligado_count = sum(predicted_classes == 1)

        print(f"Resumo: {defeito_count} defeito, {ligado_count} ligado")
        print()

        # Mostrar detalhes de cada amostra
        for i, (sample, pred_class, probs) in enumerate(zip(data, predicted_classes, predictions)):
            print(f"  Amostra {i+1}:")
            print(f"    Dados: X={sample[0]:6.0f}, Y={sample[1]:6.0f}, Z={sample[2]:6.0f}, ADC={sample[3]:4.0f}")
            print(f"    Predição: {classes[pred_class].upper()}")
            print(f"    Probabilidades: defeito={probs[0]:.2%}, ligado={probs[1]:.2%}")

            # Análise da confiança
            max_prob = max(probs)
            if max_prob > 0.95:
                confidence = "ALTA"
            elif max_prob > 0.75:
                confidence = "MÉDIA"
            else:
                confidence = "BAIXA"
            print(f"    Confiança: {confidence} ({max_prob:.1%})")
            print()

def predict_tflite(interpreter, scaler, samples):
    """Faz predições usando o modelo TFLite"""
    print("\n🔮 PREDIÇÕES COM MODELO TFLITE:")
    print("=" * 60)

    classes = ['defeito', 'ligado']

    # Obter detalhes de entrada e saída
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    print(f"Input type: {input_details[0]['dtype']}")
    print(f"Output type: {output_details[0]['dtype']}")
    print()

    for category, data in samples.items():
        print(f"\n📌 Testando dados de '{category}':")
        print("-" * 50)

        # Normalizar dados
        data_scaled = scaler.transform(data)

        # Estatísticas das predições
        defeito_count = 0
        ligado_count = 0

        for i, sample in enumerate(data):
            try:
                # Preparar entrada baseada no tipo esperado
                input_data = data_scaled[i:i+1].astype(np.float32)

                # Se o modelo espera INT8, precisamos quantizar
                if input_details[0]['dtype'] == np.int8:
                    # Obter parâmetros de quantização
                    input_scale = input_details[0]['quantization'][0]
                    input_zero_point = input_details[0]['quantization'][1]

                    # Quantizar para INT8
                    input_quantized = np.round(input_data / input_scale + input_zero_point).astype(np.int8)
                    interpreter.set_tensor(input_details[0]['index'], input_quantized)
                else:
                    interpreter.set_tensor(input_details[0]['index'], input_data)

                # Executar inferência
                interpreter.invoke()

                # Obter resultado
                output_data = interpreter.get_tensor(output_details[0]['index'])

                # Se o output é INT8, dequantizar
                if output_details[0]['dtype'] == np.int8:
                    output_scale = output_details[0]['quantization'][0]
                    output_zero_point = output_details[0]['quantization'][1]
                    output_float = (output_data.astype(np.float32) - output_zero_point) * output_scale
                    # Aplicar softmax
                    output_exp = np.exp(output_float - np.max(output_float))
                    probs = output_exp / output_exp.sum()
                    probs = probs[0]
                else:
                    probs = output_data[0]

                pred_class = np.argmax(probs)

                if pred_class == 0:
                    defeito_count += 1
                else:
                    ligado_count += 1

                print(f"  Amostra {i+1}:")
                print(f"    Dados: X={sample[0]:6.0f}, Y={sample[1]:6.0f}, Z={sample[2]:6.0f}, ADC={sample[3]:4.0f}")
                print(f"    Predição: {classes[pred_class].upper()}")
                print(f"    Probabilidades: defeito={probs[0]:.2%}, ligado={probs[1]:.2%}")
                print()

            except Exception as e:
                print(f"  Erro na amostra {i+1}: {e}")

        print(f"Resumo: {defeito_count} defeito, {ligado_count} ligado")

def test_with_real_data(model, scaler):
    """Teste com dados reais do usuário"""
    print("\n🎯 TESTE COM DADOS REAIS DO USUÁRIO:")
    print("=" * 60)

    # Dados reais que você pode inserir
    real_data = []

    print("\nInsira dados reais para teste (digite 'fim' para parar):")
    print("Formato: x,y,z,adc_raw (exemplo: -9810,-618,12151,2955)")
    print("\nExemplos de dados reais:")
    print("  Motor ligado: -9810,-618,12151,2955")
    print("  Motor defeito: -12026,-1683,9706,3057")
    print()

    while True:
        entrada = input("→ ")
        if entrada.lower() == 'fim':
            break

        try:
            valores = [float(x.strip()) for x in entrada.split(',')]
            if len(valores) == 4:
                real_data.append(valores)
                print(f"  ✅ Dados adicionados: {valores}")
            else:
                print("  ❌ Digite exatamente 4 valores separados por vírgula")
        except:
            print("  ❌ Formato inválido. Use números separados por vírgula")

    if real_data:
        real_array = np.array(real_data, dtype=np.float32)
        data_scaled = scaler.transform(real_array)
        predictions = model.predict(data_scaled, verbose=0)

        classes = ['defeito', 'ligado']

        print("\n📊 RESULTADOS:")
        print("-" * 50)
        for i, (sample, probs) in enumerate(zip(real_data, predictions)):
            pred_class = np.argmax(probs)
            print(f"\nAmostra {i+1}:")
            print(f"  Dados: X={sample[0]:6.0f}, Y={sample[1]:6.0f}, Z={sample[2]:6.0f}, ADC={sample[3]:4.0f}")
            print(f"  Classificação: {classes[pred_class].upper()}")
            print(f"  Probabilidades:")
            print(f"    - Motor com defeito: {probs[0]:.1%}")
            print(f"    - Motor ligado normal: {probs[1]:.1%}")

            # Interpretação
            max_prob = max(probs)
            if max_prob > 0.9:
                print(f"  📌 Alta confiança na classificação! ({max_prob:.1%})")
            elif max_prob > 0.7:
                print(f"  📌 Confiança moderada na classificação ({max_prob:.1%})")
            else:
                print(f"  ⚠️ Baixa confiança - resultado incerto ({max_prob:.1%})")

def diagnose_model_issue(model, scaler, samples):
    """Diagnostica possíveis problemas com o modelo"""
    print("\n🔍 DIAGNÓSTICO DO MODELO:")
    print("=" * 60)

    all_predictions = []
    all_data = []

    for category, data in samples.items():
        data_scaled = scaler.transform(data)
        predictions = model.predict(data_scaled, verbose=0)
        all_predictions.extend(predictions)
        all_data.extend(data)

    all_predictions = np.array(all_predictions)
    predicted_classes = np.argmax(all_predictions, axis=1)

    # Análise das predições
    unique_classes = np.unique(predicted_classes)
    print(f"Classes preditas únicas: {unique_classes}")
    print(f"Distribuição das predições:")
    print(f"  - Classe 0 (defeito): {sum(predicted_classes == 0)} amostras")
    print(f"  - Classe 1 (ligado): {sum(predicted_classes == 1)} amostras")

    # Verificar variação nas probabilidades
    prob_std = np.std(all_predictions, axis=0)
    print(f"\nDesvio padrão das probabilidades:")
    print(f"  - Classe defeito: {prob_std[0]:.4f}")
    print(f"  - Classe ligado: {prob_std[1]:.4f}")

    if len(unique_classes) == 1:
        print("\n⚠️ PROBLEMA DETECTADO:")
        print("O modelo está classificando todas as amostras na mesma classe!")
        print("\nPossíveis causas:")
        print("1. Dados de treino desbalanceados")
        print("2. Overfitting em uma classe dominante")
        print("3. Problemas na normalização dos dados")
        print("4. Learning rate muito alto/baixo durante o treino")

        print("\nRecomendações:")
        print("1. Verificar se os dados de treino tinham proporção similar de cada classe")
        print("2. Re-treinar com class_weight='balanced'")
        print("3. Verificar se o scaler foi treinado com todos os dados")
        print("4. Aumentar a diversidade dos dados de treino")

def main():
    """Função principal"""
    print("🚀 TESTE DO MODELO MOTOR CLASSIFIER")
    print("=" * 60)
    print(f"Model ID: {MODEL_ID}")
    print(f"Features: x, y, z, adc_raw")
    print(f"Classes: defeito, ligado")
    print(f"Accuracy reportada: 99.77%")
    print("=" * 60)

    # 1. Baixar arquivos do modelo
    files = download_model_files()

    if 'model' not in files or 'scaler' not in files:
        print("\n❌ Não foi possível baixar todos os arquivos necessários")
        print("Certifique-se que o servidor Django está rodando em http://127.0.0.1:8000")
        return

    # 2. Carregar modelo Keras
    model, scaler = load_keras_model(files['model'], files['scaler'])

    # 3. Mostrar resumo do modelo
    print("\n📋 Resumo do Modelo:")
    print("-" * 50)
    model.summary()

    # 4. Gerar dados de teste baseados em dados reais
    test_samples = generate_test_data()

    # 5. Analisar características dos dados
    analyze_data_characteristics(test_samples)

    # 6. Testar com Keras
    predict_keras(model, scaler, test_samples)

    # 7. Diagnosticar possíveis problemas
    diagnose_model_issue(model, scaler, test_samples)

    # 8. Testar com TFLite (se disponível)
    if 'tflite' in files:
        try:
            interpreter = load_tflite_model(files['tflite'])
            predict_tflite(interpreter, scaler, test_samples)
        except Exception as e:
            print(f"\n⚠️ Erro ao testar TFLite: {e}")
            print("Continuando com modelo Keras apenas...")

    # 9. Teste com dados reais
    print("\n" + "=" * 60)
    resposta = input("\n🔬 Deseja testar com seus próprios dados? (s/n): ")
    if resposta.lower() == 's':
        test_with_real_data(model, scaler)

    print("\n✅ TESTE CONCLUÍDO!")
    print("=" * 60)


def extract_scaler_params():
    """Extrai parâmetros do scaler para implementação em C/C++"""
    import joblib
    import os

    # Caminho local do scaler
    scaler_path = os.path.join(MODELS_DIR, 'scaler.pkl')

    # Verificar se o arquivo existe
    if not os.path.exists(scaler_path):
        print(f"❌ Arquivo do scaler não encontrado: {scaler_path}")
        print("Execute primeiro a função main() para baixar os arquivos.")
        return None, None

    # Carregar o scaler
    scaler = joblib.load(scaler_path)

    # Extrair parâmetros
    means = scaler.mean_
    scales = scaler.scale_

    print("🔧 PARÂMETROS DO SCALER EXTRAÍDOS:")
    print("=" * 50)
    print("// Parâmetros do StandardScaler para C/C++")
    print("const float SCALER_MEAN[4] = {")
    print(f"    {means[0]:.6f}f,  // X")
    print(f"    {means[1]:.6f}f,  // Y")
    print(f"    {means[2]:.6f}f,  // Z")
    print(f"    {means[3]:.6f}f   // ADC_RAW")
    print("};")
    print()
    print("const float SCALER_SCALE[4] = {")
    print(f"    {scales[0]:.6f}f,  // X")
    print(f"    {scales[1]:.6f}f,  // Y")
    print(f"    {scales[2]:.6f}f,  // Z")
    print(f"    {scales[3]:.6f}f   // ADC_RAW")
    print("};")

    return means, scales

def download_and_extract_scaler():
    """Baixa os arquivos e extrai os parâmetros do scaler"""
    print("🚀 EXTRAINDO PARÂMETROS DO SCALER PARA MICROCONTROLADOR")
    print("=" * 60)

    # Baixar arquivos
    files = download_model_files()

    if 'scaler' not in files:
        print("\n❌ Não foi possível baixar o arquivo do scaler")
        return None, None

    # Extrair parâmetros
    return extract_scaler_params()


if __name__ == "__main__":
    #main()
    download_and_extract_scaler()
