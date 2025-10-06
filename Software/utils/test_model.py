"""
Script de teste para os modelos treinados (Keras e TFLite)
Simula dados do microcontrolador e testa as predições
"""

import tensorflow as tf
import numpy as np
import pandas as pd
from pathlib import Path
import os
import time
import matplotlib.pyplot as plt
import seaborn as sns
from sklearn.metrics import confusion_matrix, classification_report

# Configurações de caminhos
BASE_DIR = Path(__file__).resolve().parent.parent
MODEL_DIR = BASE_DIR / "models"
DATA_DIR = BASE_DIR / "data" / "csv_files"

# Nomes dos modelos
MODEL_NAME = "sdfpm_motor_v1"
KERAS_MODEL_PATH = MODEL_DIR / f"{MODEL_NAME}_best.h5"
TFLITE_MODEL_PATH = MODEL_DIR / f"{MODEL_NAME}.tflite"

# Classes
CLASS_NAMES = {
    0: "Desligado",
    1: "Ligado",
    2: "Defeito"
}

class MotorDataSimulator:
    """Simula dados do microcontrolador baseado nas estatísticas reais"""

    def __init__(self):
        # Estatísticas baseadas nos dados reais
        self.stats = {
            'desligado': {  # status = 0
                'x': {'mean': -9846, 'std': 500},
                'y': {'mean': -556, 'std': 200},
                'z': {'mean': 12137, 'std': 300},
                'adc_raw': {'mean': 2956, 'std': 20}
            },
            'ligado': {  # status = 1
                'x': {'mean': -9806, 'std': 400},
                'y': {'mean': -512, 'std': 250},
                'z': {'mean': 12194, 'std': 350},
                'adc_raw': {'mean': 2969, 'std': 25}
            },
            'defeito': {  # status = 2
                'x': {'mean': -10200, 'std': 1500},
                'y': {'mean': -300, 'std': 600},
                'z': {'mean': 11800, 'std': 1800},
                'adc_raw': {'mean': 3020, 'std': 60}
            }
        }

    def generate_sample(self, status='ligado', noise_level=1.0):
        """
        Gera uma amostra simulada

        Args:
            status (str): 'desligado', 'ligado' ou 'defeito'
            noise_level (float): Multiplicador do ruído (padrão=1.0)

        Returns:
            np.array: Amostra com formato [x, y, z, adc_raw]
        """
        stats = self.stats[status]

        sample = np.array([
            np.random.normal(stats['x']['mean'], stats['x']['std'] * noise_level),
            np.random.normal(stats['y']['mean'], stats['y']['std'] * noise_level),
            np.random.normal(stats['z']['mean'], stats['z']['std'] * noise_level),
            np.random.normal(stats['adc_raw']['mean'], stats['adc_raw']['std'] * noise_level)
        ])

        return sample

    def generate_batch(self, n_samples=100, status='ligado', noise_level=1.0):
        """Gera um lote de amostras"""
        samples = [self.generate_sample(status, noise_level) for _ in range(n_samples)]
        return np.array(samples)

    def generate_mixed_batch(self, n_samples=300):
        """Gera um lote com amostras de todas as classes"""
        samples = []
        labels = []

        status_map = {'desligado': 0, 'ligado': 1, 'defeito': 2}

        for status_name, status_id in status_map.items():
            batch = self.generate_batch(n_samples // 3, status_name)
            samples.append(batch)
            labels.extend([status_id] * (n_samples // 3))

        samples = np.vstack(samples)
        labels = np.array(labels)

        # Embaralhar
        indices = np.random.permutation(len(samples))
        return samples[indices], labels[indices]

    def simulate_realtime_stream(self, status='ligado', duration_sec=5, samples_per_sec=100):
        """
        Simula um stream de dados em tempo real

        Args:
            status (str): Status do motor
            duration_sec (int): Duração da simulação em segundos
            samples_per_sec (int): Amostras por segundo

        Returns:
            list: Lista de amostras
        """
        n_samples = duration_sec * samples_per_sec
        samples = self.generate_batch(n_samples, status)
        return samples


class ModelTester:
    """Testa modelos Keras e TFLite"""

    def __init__(self):
        self.keras_model = None
        self.tflite_interpreter = None
        self.simulator = MotorDataSimulator()

    def load_keras_model(self, model_path):
        """Carrega modelo Keras"""
        print(f"📥 Carregando modelo Keras: {model_path}")
        if not os.path.exists(model_path):
            raise FileNotFoundError(f"Modelo não encontrado: {model_path}")

        self.keras_model = tf.keras.models.load_model(model_path)
        print("✅ Modelo Keras carregado com sucesso!")
        print(f"   - Input shape: {self.keras_model.input_shape}")
        print(f"   - Output shape: {self.keras_model.output_shape}")
        return self.keras_model

    def load_tflite_model(self, model_path):
        """Carrega modelo TFLite"""
        print(f"📥 Carregando modelo TFLite: {model_path}")
        if not os.path.exists(model_path):
            raise FileNotFoundError(f"Modelo não encontrado: {model_path}")

        self.tflite_interpreter = tf.lite.Interpreter(model_path=str(model_path))
        self.tflite_interpreter.allocate_tensors()

        # Informações sobre input/output
        input_details = self.tflite_interpreter.get_input_details()
        output_details = self.tflite_interpreter.get_output_details()

        print("✅ Modelo TFLite carregado com sucesso!")
        print(f"   - Input shape: {input_details[0]['shape']}")
        print(f"   - Input dtype: {input_details[0]['dtype']}")
        print(f"   - Output shape: {output_details[0]['shape']}")
        print(f"   - Output dtype: {output_details[0]['dtype']}")

        return self.tflite_interpreter

    def predict_keras(self, samples):
        """Predição usando modelo Keras"""
        if self.keras_model is None:
            raise ValueError("Modelo Keras não carregado!")

        # Garantir que samples é 2D
        if len(samples.shape) == 1:
            samples = samples.reshape(1, -1)

        predictions = self.keras_model.predict(samples, verbose=0)
        predicted_classes = np.argmax(predictions, axis=1)

        return predicted_classes, predictions

    def predict_tflite(self, samples):
        """Predição usando modelo TFLite"""
        if self.tflite_interpreter is None:
            raise ValueError("Modelo TFLite não carregado!")

        input_details = self.tflite_interpreter.get_input_details()
        output_details = self.tflite_interpreter.get_output_details()

        # Garantir que samples é 2D
        if len(samples.shape) == 1:
            samples = samples.reshape(1, -1)

        predictions = []

        for sample in samples:
            # Preparar input (quantização se necessário)
            if input_details[0]['dtype'] == np.int8:
                # Quantizar input
                input_scale, input_zero_point = input_details[0]['quantization']
                sample_quantized = sample / input_scale + input_zero_point
                sample_quantized = sample_quantized.astype(np.int8)
                input_data = sample_quantized.reshape(input_details[0]['shape'])
            else:
                input_data = sample.astype(np.float32).reshape(input_details[0]['shape'])

            # Fazer predição
            self.tflite_interpreter.set_tensor(input_details[0]['index'], input_data)
            self.tflite_interpreter.invoke()
            output_data = self.tflite_interpreter.get_tensor(output_details[0]['index'])

            # Dequantizar output se necessário
            if output_details[0]['dtype'] == np.int8:
                output_scale, output_zero_point = output_details[0]['quantization']
                output_data = (output_data.astype(np.float32) - output_zero_point) * output_scale

            predictions.append(output_data[0])

        predictions = np.array(predictions)
        predicted_classes = np.argmax(predictions, axis=1)

        return predicted_classes, predictions

    def test_single_sample(self, status='ligado'):
        """Testa uma única amostra"""
        print(f"\n{'='*60}")
        print(f"🧪 TESTE COM AMOSTRA ÚNICA - Status: {status.upper()}")
        print(f"{'='*60}")

        # Gerar amostra
        sample = self.simulator.generate_sample(status)
        print(f"\n📊 Dados simulados do microcontrolador:")
        print(f"   X: {sample[0]:.2f}")
        print(f"   Y: {sample[1]:.2f}")
        print(f"   Z: {sample[2]:.2f}")
        print(f"   ADC_RAW: {sample[3]:.2f}")

        # Testar com Keras
        if self.keras_model:
            print(f"\n🔮 Predição Keras:")
            pred_class, pred_probs = self.predict_keras(sample)
            print(f"   Classe predita: {pred_class[0]} ({CLASS_NAMES[pred_class[0]]})")
            print(f"   Probabilidades:")
            for i, prob in enumerate(pred_probs[0]):
                print(f"      {CLASS_NAMES[i]}: {prob*100:.2f}%")

        # Testar com TFLite
        if self.tflite_interpreter:
            print(f"\n🔮 Predição TFLite:")
            pred_class, pred_probs = self.predict_tflite(sample)
            print(f"   Classe predita: {pred_class[0]} ({CLASS_NAMES[pred_class[0]]})")
            print(f"   Probabilidades:")
            for i, prob in enumerate(pred_probs[0]):
                print(f"      {CLASS_NAMES[i]}: {prob*100:.2f}%")

    def test_batch(self, n_samples=300):
        """Testa um lote de amostras mistas"""
        print(f"\n{'='*60}")
        print(f"🧪 TESTE COM LOTE DE {n_samples} AMOSTRAS")
        print(f"{'='*60}")

        # Gerar amostras
        samples, true_labels = self.simulator.generate_mixed_batch(n_samples)

        print(f"\n📊 Distribuição de amostras geradas:")
        for i, name in CLASS_NAMES.items():
            count = np.sum(true_labels == i)
            print(f"   {name}: {count}")

        # Testar com Keras
        if self.keras_model:
            print(f"\n🔮 Testando com Keras...")
            start_time = time.time()
            pred_classes_keras, _ = self.predict_keras(samples)
            keras_time = time.time() - start_time

            accuracy_keras = np.mean(pred_classes_keras == true_labels)
            print(f"✅ Acurácia Keras: {accuracy_keras*100:.2f}%")
            print(f"⏱️  Tempo total: {keras_time*1000:.2f}ms")
            print(f"⏱️  Tempo por amostra: {(keras_time/n_samples)*1000:.2f}ms")

            print(f"\n📊 Relatório de Classificação (Keras):")
            print(classification_report(true_labels, pred_classes_keras,
                                       target_names=list(CLASS_NAMES.values())))

        # Testar com TFLite
        if self.tflite_interpreter:
            print(f"\n🔮 Testando com TFLite...")
            start_time = time.time()
            pred_classes_tflite, _ = self.predict_tflite(samples)
            tflite_time = time.time() - start_time

            accuracy_tflite = np.mean(pred_classes_tflite == true_labels)
            print(f"✅ Acurácia TFLite: {accuracy_tflite*100:.2f}%")
            print(f"⏱️  Tempo total: {tflite_time*1000:.2f}ms")
            print(f"⏱️  Tempo por amostra: {(tflite_time/n_samples)*1000:.2f}ms")

            print(f"\n📊 Relatório de Classificação (TFLite):")
            print(classification_report(true_labels, pred_classes_tflite,
                                       target_names=list(CLASS_NAMES.values())))

            # Plotar matriz de confusão
            self._plot_confusion_matrices(true_labels, pred_classes_keras, pred_classes_tflite)

        return {
            'keras': {'accuracy': accuracy_keras, 'time': keras_time} if self.keras_model else None,
            'tflite': {'accuracy': accuracy_tflite, 'time': tflite_time} if self.tflite_interpreter else None
        }

    def test_realtime_simulation(self, duration_sec=5, samples_per_sec=100):
        """Simula predição em tempo real"""
        print(f"\n{'='*60}")
        print(f"🧪 SIMULAÇÃO DE TEMPO REAL")
        print(f"{'='*60}")
        print(f"Duração: {duration_sec}s | Taxa: {samples_per_sec} amostras/seg")

        # Simular transição entre estados
        statuses = ['desligado', 'ligado', 'defeito']

        for status in statuses:
            print(f"\n🔄 Simulando motor {status.upper()}...")
            samples = self.simulator.simulate_realtime_stream(
                status, duration_sec=2, samples_per_sec=samples_per_sec
            )

            # Predição com Keras
            if self.keras_model:
                pred_classes, pred_probs = self.predict_keras(samples)

                # Estatísticas
                unique, counts = np.unique(pred_classes, return_counts=True)
                print(f"   Predições:")
                for cls, count in zip(unique, counts):
                    percentage = (count / len(pred_classes)) * 100
                    print(f"      {CLASS_NAMES[cls]}: {count}/{len(pred_classes)} ({percentage:.1f}%)")

                # Confiança média
                max_probs = np.max(pred_probs, axis=1)
                print(f"   Confiança média: {np.mean(max_probs)*100:.2f}%")

    def _plot_confusion_matrices(self, true_labels, pred_keras, pred_tflite):
        """Plota matrizes de confusão"""
        fig, axes = plt.subplots(1, 2, figsize=(14, 5))

        # Matriz de confusão Keras
        cm_keras = confusion_matrix(true_labels, pred_keras)
        sns.heatmap(cm_keras, annot=True, fmt='d', cmap='Blues',
                   xticklabels=CLASS_NAMES.values(),
                   yticklabels=CLASS_NAMES.values(),
                   ax=axes[0])
        axes[0].set_title('Matriz de Confusão - Keras')
        axes[0].set_ylabel('Verdadeiro')
        axes[0].set_xlabel('Predito')

        # Matriz de confusão TFLite
        cm_tflite = confusion_matrix(true_labels, pred_tflite)
        sns.heatmap(cm_tflite, annot=True, fmt='d', cmap='Greens',
                   xticklabels=CLASS_NAMES.values(),
                   yticklabels=CLASS_NAMES.values(),
                   ax=axes[1])
        axes[1].set_title('Matriz de Confusão - TFLite')
        axes[1].set_ylabel('Verdadeiro')
        axes[1].set_xlabel('Predito')

        plt.tight_layout()

        # Salvar
        save_path = MODEL_DIR / 'confusion_matrices.png'
        plt.savefig(save_path, dpi=300, bbox_inches='tight')
        print(f"\n✅ Matrizes de confusão salvas em: {save_path}")
        plt.show()

    def compare_models(self, n_samples=1000):
        """Compara os dois modelos lado a lado"""
        print(f"\n{'='*60}")
        print(f"📊 COMPARAÇÃO ENTRE MODELOS")
        print(f"{'='*60}")

        if not (self.keras_model and self.tflite_interpreter):
            print("⚠️  Ambos os modelos precisam estar carregados para comparação!")
            return

        # Gerar amostras de teste
        samples, true_labels = self.simulator.generate_mixed_batch(n_samples)

        # Keras
        start = time.time()
        pred_keras, probs_keras = self.predict_keras(samples)
        keras_time = time.time() - start
        keras_acc = np.mean(pred_keras == true_labels)

        # TFLite
        start = time.time()
        pred_tflite, probs_tflite = self.predict_tflite(samples)
        tflite_time = time.time() - start
        tflite_acc = np.mean(pred_tflite == true_labels)

        # Concordância entre modelos
        agreement = np.mean(pred_keras == pred_tflite)

        print(f"\n📊 Resultados ({n_samples} amostras):")
        print(f"\n   Keras:")
        print(f"      Acurácia: {keras_acc*100:.2f}%")
        print(f"      Tempo: {keras_time*1000:.2f}ms ({keras_time/n_samples*1000:.3f}ms/amostra)")

        print(f"\n   TFLite:")
        print(f"      Acurácia: {tflite_acc*100:.2f}%")
        print(f"      Tempo: {tflite_time*1000:.2f}ms ({tflite_time/n_samples*1000:.3f}ms/amostra)")
        print(f"      Speedup: {keras_time/tflite_time:.2f}x")

        print(f"\n   Concordância entre modelos: {agreement*100:.2f}%")

        # Diferenças nas probabilidades
        prob_diff = np.abs(probs_keras - probs_tflite).mean()
        print(f"   Diferença média nas probabilidades: {prob_diff:.4f}")


def main():
    """Função principal para executar todos os testes"""
    print("="*60)
    print("🚀 SISTEMA DE TESTE DE MODELO DE MOTOR")
    print("="*60)

    # Inicializar testador
    tester = ModelTester()

    # Carregar modelos
    try:
        tester.load_keras_model(KERAS_MODEL_PATH)
    except FileNotFoundError as e:
        print(f"⚠️  {e}")

    try:
        tester.load_tflite_model(TFLITE_MODEL_PATH)
    except FileNotFoundError as e:
        print(f"⚠️  {e}")

    if not (tester.keras_model or tester.tflite_interpreter):
        print("\n❌ Nenhum modelo disponível para teste!")
        return

    # Executar testes
    print("\n" + "="*60)
    print("📋 MENU DE TESTES")
    print("="*60)

    # Teste 1: Amostras únicas
    print("\n\n### TESTE 1: Amostras Únicas ###")
    for status in ['desligado', 'ligado', 'defeito']:
        tester.test_single_sample(status)

    # Teste 2: Lote de amostras
    print("\n\n### TESTE 2: Lote de Amostras ###")
    tester.test_batch(n_samples=300)

    # Teste 3: Simulação tempo real
    print("\n\n### TESTE 3: Simulação Tempo Real ###")
    tester.test_realtime_simulation(duration_sec=3, samples_per_sec=100)

    # Teste 4: Comparação de modelos
    if tester.keras_model and tester.tflite_interpreter:
        print("\n\n### TESTE 4: Comparação de Modelos ###")
        tester.compare_models(n_samples=1000)

    print("\n\n" + "="*60)
    print("✅ TESTES CONCLUÍDOS!")
    print("="*60)


if __name__ == "__main__":
    main()

