import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers, Sequential
import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler, LabelEncoder
from sklearn.metrics import classification_report, confusion_matrix
import matplotlib.pyplot as plt
import seaborn as sns
import os
from datetime import datetime
from pathlib import Path
import os

CSV_PATH = Path(__file__).resolve().parent.parent / "data" / "csv_files" / "motor_data_training.csv"

class MotorCNNTrainer:
    """
    Classe para treinamento de modelo CNN 1D para classificação de estados do motor.
    Estados: 0=desligado, 1=ligado, 2=defeito
    """
    
    def __init__(self, model_name="motor_classifier", save_dir="models"):
        self.model_name = model_name
        self.save_dir = save_dir
        self.model = None
        self.scaler = None
        self.history = None
        self.X_train = None
        self.X_test = None
        self.y_train = None
        self.y_test = None
        self.feature_names = None
        
        # Criar diretório se não existir
        os.makedirs(save_dir, exist_ok=True)
    
    def load_data(self, df, feature_columns=None, target_column='label', test_size=0.2, random_state=42):
        """
        Carrega e prepara os dados para treinamento
        
        Args:
            df (DataFrame): DataFrame com os dados processados
            feature_columns (list): Lista com nomes das colunas de features
            target_column (str): Nome da coluna target
            test_size (float): Proporção dos dados para teste
            random_state (int): Seed para reprodutibilidade
        """
        print("📊 Carregando dados...")
        
        # Features padrão se não especificadas
        if feature_columns is None:
            feature_columns = ['x', 'y', 'z', 'magnitude', 'x_abs', 'y_abs', 'z_abs']
        
        # Verificar se as colunas existem
        missing_cols = [col for col in feature_columns if col not in df.columns]
        if missing_cols:
            raise ValueError(f"Colunas não encontradas: {missing_cols}")
        
        self.feature_names = feature_columns
        
        # Separar features e target
        X = df[feature_columns].values
        y = df[target_column].values
        
        # Split train/test
        self.X_train, self.X_test, self.y_train, self.y_test = train_test_split(
            X, y, test_size=test_size, random_state=random_state, stratify=y
        )
        
        print(f"✅ Dados carregados:")
        print(f"   - Features: {len(feature_columns)} ({feature_columns})")
        print(f"   - Samples total: {len(X)}")
        print(f"   - Train: {len(self.X_train)} | Test: {len(self.X_test)}")
        print(f"   - Classes: {np.unique(y)}")
        
    def normalize_data(self):
        """Normaliza os dados usando StandardScaler"""
        print("🔄 Normalizando dados...")
        
        self.scaler = StandardScaler()
        self.X_train = self.scaler.fit_transform(self.X_train)
        self.X_test = self.scaler.transform(self.X_test)
        
        print("✅ Dados normalizados")
    
    def build_model(self, input_shape=None, num_classes=3, 
                   conv_filters=[32, 64], dense_units=50, dropout_rate=0.3):
        """
        Constrói o modelo CNN 1D
        
        Args:
            input_shape (int): Número de features de entrada
            num_classes (int): Número de classes (desligado, ligado, defeito)
            conv_filters (list): Número de filtros para cada camada Conv1D
            dense_units (int): Unidades na camada dense
            dropout_rate (float): Taxa de dropout
        """
        print("🏗️ Construindo modelo CNN 1D...")
        
        if input_shape is None:
            input_shape = self.X_train.shape[1]
        
        self.model = Sequential([
            # Reshape para CNN 1D
            layers.Reshape((input_shape, 1), input_shape=(input_shape,)),
            
            # Primeira camada convolucional
            layers.Conv1D(conv_filters[0], 3, activation='relu', padding='same'),
            layers.BatchNormalization(),
            layers.MaxPooling1D(2),
            
            # Segunda camada convolucional
            layers.Conv1D(conv_filters[1], 3, activation='relu', padding='same'),
            layers.BatchNormalization(),
            layers.GlobalAveragePooling1D(),
            
            # Camadas densas
            layers.Dense(dense_units, activation='relu'),
            layers.Dropout(dropout_rate),
            layers.Dense(num_classes, activation='softmax')
        ])
        
        # Compilar modelo
        self.model.compile(
            optimizer='adam',
            loss='sparse_categorical_crossentropy',
            metrics=['accuracy']
        )
        
        print("✅ Modelo construído:")
        self.model.summary()
    
    def train_model(self, epochs=100, batch_size=32, validation_split=0.2, 
                   early_stopping=True, patience=10, verbose=1):
        """
        Treina o modelo
        
        Args:
            epochs (int): Número máximo de épocas
            batch_size (int): Tamanho do batch
            validation_split (float): Proporção para validação
            early_stopping (bool): Usar early stopping
            patience (int): Paciência para early stopping
            verbose (int): Verbosidade do treinamento
        """
        print("🚀 Iniciando treinamento...")
        
        callbacks = []
        
        if early_stopping:
            early_stop = keras.callbacks.EarlyStopping(
                monitor='val_loss',
                patience=patience,
                restore_best_weights=True,
                verbose=1
            )
            callbacks.append(early_stop)
        
        # Callback para salvar melhor modelo
        checkpoint = keras.callbacks.ModelCheckpoint(
            filepath=os.path.join(self.save_dir, f'{self.model_name}_best.h5'),
            monitor='val_accuracy',
            save_best_only=True,
            verbose=1
        )
        callbacks.append(checkpoint)
        
        # Treinar modelo
        self.history = self.model.fit(
            self.X_train, self.y_train,
            epochs=epochs,
            batch_size=batch_size,
            validation_split=validation_split,
            callbacks=callbacks,
            verbose=verbose
        )
        
        print("✅ Treinamento concluído!")
    
    def evaluate_model(self):
        """Avalia o modelo nos dados de teste"""
        print("📈 Avaliando modelo...")
        
        # Predições
        y_pred = self.model.predict(self.X_test)
        y_pred_classes = np.argmax(y_pred, axis=1)
        
        # Métricas
        test_loss, test_accuracy = self.model.evaluate(self.X_test, self.y_test, verbose=0)
        
        print(f"✅ Resultados no teste:")
        print(f"   - Acurácia: {test_accuracy:.4f}")
        print(f"   - Loss: {test_loss:.4f}")
        
        # Relatório detalhado
        class_names = ['Desligado', 'Ligado', 'Defeito']
        print("\n📊 Relatório de Classificação:")
        print(classification_report(self.y_test, y_pred_classes, 
                                  target_names=class_names))
        
        return test_accuracy, test_loss
    
    def plot_training_history(self, save_plot=True):
        """Plota histórico do treinamento"""
        print("📊 Gerando gráficos do treinamento...")
        
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 4))
        
        # Acurácia
        ax1.plot(self.history.history['accuracy'], label='Train')
        ax1.plot(self.history.history['val_accuracy'], label='Validation')
        ax1.set_title('Acurácia do Modelo')
        ax1.set_xlabel('Época')
        ax1.set_ylabel('Acurácia')
        ax1.legend()
        ax1.grid(True)
        
        # Loss
        ax2.plot(self.history.history['loss'], label='Train')
        ax2.plot(self.history.history['val_loss'], label='Validation')
        ax2.set_title('Loss do Modelo')
        ax2.set_xlabel('Época')
        ax2.set_ylabel('Loss')
        ax2.legend()
        ax2.grid(True)
        
        plt.tight_layout()
        
        if save_plot:
            plot_path = os.path.join(self.save_dir, f'{self.model_name}_training.png')
            plt.savefig(plot_path, dpi=300, bbox_inches='tight')
            print(f"✅ Gráfico salvo: {plot_path}")
        
        plt.show()
    
    def save_model(self, save_scaler=True):
        """Salva o modelo treinado"""
        print("💾 Salvando modelo...")
        
        # Salvar modelo Keras
        model_path = os.path.join(self.save_dir, f'{self.model_name}.h5')
        self.model.save(model_path)
        
        # Salvar scaler
        if save_scaler and self.scaler:
            import joblib
            scaler_path = os.path.join(self.save_dir, f'{self.model_name}_scaler.pkl')
            joblib.dump(self.scaler, scaler_path)
            print(f"✅ Scaler salvo: {scaler_path}")
        
        print(f"✅ Modelo salvo: {model_path}")
        
        # Informações do modelo
        info = {
            'model_name': self.model_name,
            'features': self.feature_names,
            'input_shape': self.X_train.shape[1],
            'num_classes': 3,
            'timestamp': datetime.now().isoformat()
        }
        
        info_path = os.path.join(self.save_dir, f'{self.model_name}_info.txt')
        with open(info_path, 'w') as f:
            for key, value in info.items():
                f.write(f"{key}: {value}\n")
        
        return model_path
    
    def convert_to_tflite(self, quantize=True):
        """
        Converte modelo para TensorFlow Lite (ESP32)
        
        Args:
            quantize (bool): Aplicar quantização int8
        """
        print("🔄 Convertendo para TensorFlow Lite...")
        
        # Converter
        converter = tf.lite.TFLiteConverter.from_keras_model(self.model)
        
        if quantize:
            # Otimizações para ESP32
            converter.optimizations = [tf.lite.Optimize.DEFAULT]
            
            # Dataset representativo para quantização
            def representative_data_gen():
                for i in range(min(100, len(self.X_test))):
                    yield [self.X_test[i:i+1].astype(np.float32)]
            
            converter.representative_dataset = representative_data_gen
            converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
            converter.inference_input_type = tf.int8
            converter.inference_output_type = tf.int8
        
        tflite_model = converter.convert()
        
        # Salvar modelo TFLite
        tflite_path = os.path.join(self.save_dir, f'{self.model_name}.tflite')
        with open(tflite_path, 'wb') as f:
            f.write(tflite_model)
        
        # Mostrar tamanho do modelo
        size_kb = len(tflite_model) / 1024
        print(f"✅ Modelo TFLite salvo: {tflite_path}")
        print(f"   - Tamanho: {size_kb:.2f} KB")
        print(f"   - Quantizado: {'Sim' if quantize else 'Não'}")
        
        return tflite_path
    
    def train_complete_pipeline(self, df, feature_columns=None, target_column='label',
                               epochs=100, batch_size=32, convert_tflite=True):
        """
        Executa o pipeline completo de treinamento
        
        Args:
            df (DataFrame): DataFrame com dados processados
            feature_columns (list): Colunas de features
            target_column (str): Coluna target
            epochs (int): Épocas de treinamento
            batch_size (int): Tamanho do batch
            convert_tflite (bool): Converter para TFLite
            
        Returns:
            dict: Informações do modelo treinado
        """
        print("🎯 INICIANDO PIPELINE COMPLETO DE TREINAMENTO")
        print("="*50)
        
        try:
            # 1. Carregar dados
            self.load_data(df, feature_columns, target_column)
            
            # 2. Normalizar
            self.normalize_data()
            
            # 3. Construir modelo
            self.build_model()
            
            # 4. Treinar
            self.train_model(epochs=epochs, batch_size=batch_size)
            
            # 5. Avaliar
            accuracy, loss = self.evaluate_model()
            
            # 6. Plotar histórico
            self.plot_training_history()
            
            # 7. Salvar modelo
            model_path = self.save_model()
            
            # 8. Converter para TFLite
            tflite_path = None
            if convert_tflite:
                tflite_path = self.convert_to_tflite()
            
            print("="*50)
            print("🎉 PIPELINE CONCLUÍDO COM SUCESSO!")
            
            # Retornar informações
            results = {
                'model': self.model,
                'scaler': self.scaler,
                'accuracy': accuracy,
                'loss': loss,
                'model_path': model_path,
                'tflite_path': tflite_path,
                'feature_names': self.feature_names
            }
            
            return results
            
        except Exception as e:
            print(f"❌ Erro durante o treinamento: {str(e)}")
            raise e

if __name__ == "__main__":
    # Exemplo de como usar a classe

    # Criar instância do trainer
    trainer = MotorCNNTrainer(model_name="sdfpm_motor_v1")

    if not os.path.exists(CSV_PATH):
        print(f"Arquivo não encontrado: {CSV_PATH}")
    else:
        df = pd.read_csv(CSV_PATH)
        print(df.head())

    results = trainer.train_complete_pipeline(
        df=df,
        feature_columns=['x', 'y', 'z'],
        target_column='status',
        batch_size=8, epochs=10,
        convert_tflite=True
    )

    # Acessar modelo treinado
    trained_model = results['model']
    print(f"Modelo treinado com acurácia: {results['accuracy']:.4f}")
