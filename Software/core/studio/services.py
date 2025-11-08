import pandas as pd
import numpy as np
import re
import os
from pathlib import Path
from scipy import stats
from typing import List, Dict, Optional, Tuple
import io

# Imports para Machine Learning
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers, Sequential
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler, LabelEncoder
from sklearn.metrics import classification_report, confusion_matrix
import joblib
from datetime import datetime


class GenericDataProcessor:
    """
    Processador genérico de dados para qualquer tipo de dataset.
    Suporta arquivos CSV e TXT com delimitadores configuráveis.
    """

    def __init__(self):
        self.data = None
        self.column_types = {}
        self.statistics = {}

    def parse_file(self, file_path: str, delimiter: str = ',',
                   file_type: str = 'csv', has_headers: bool = True) -> pd.DataFrame:
        """
        Processa um arquivo CSV ou TXT de forma genérica

        Args:
            file_path: Caminho para o arquivo
            delimiter: Caractere separador (padrão: ',')
            file_type: Tipo do arquivo ('csv' ou 'txt')
            has_headers: Se o arquivo possui cabeçalho

        Returns:
            DataFrame com os dados processados
        """
        try:
            # Primeiro, tentar detectar se é formato sensor
            is_sensor_format = self._detect_sensor_format(file_path, delimiter)

            if is_sensor_format:
                # Usar parser específico para formato sensor
                df = self._parse_sensor_file(file_path, delimiter)
            elif file_type == 'csv' or delimiter in [',', '\t', '|']:
                # Ler como CSV com delimitador específico
                df = pd.read_csv(
                    file_path,
                    delimiter=delimiter,
                    header=0 if has_headers else None,
                    encoding='utf-8',
                    on_bad_lines='skip'
                )
            else:
                # TXT genérico - tentar detectar estrutura
                df = self._parse_txt_file(file_path, delimiter, has_headers)

            # Se não tem headers, criar nomes genéricos
            if not has_headers and not is_sensor_format:
                df.columns = [f'col_{i}' for i in range(len(df.columns))]

            # Limpar nomes das colunas
            df.columns = [str(col).strip() for col in df.columns]

            return df

        except Exception as e:
            raise ValueError(f"Erro ao processar arquivo: {str(e)}")

    def _detect_sensor_format(self, file_path: str, delimiter: str) -> bool:
        """
        Detecta se o arquivo está no formato sensor (KEY value;KEY value...)
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                # Ler algumas linhas para detectar o padrão
                sample_lines = []
                for i, line in enumerate(f):
                    if i >= 5:  # Amostrar apenas 5 linhas
                        break
                    line = line.strip()
                    if line:
                        sample_lines.append(line)

                if not sample_lines:
                    return False

                # Verificar se todas as linhas seguem o padrão sensor
                for line in sample_lines:
                    if delimiter in line:
                        parts = line.split(delimiter)
                        # Verificar se pelo menos metade dos parts segue o padrão KEY value
                        matches = 0
                        for part in parts:
                            if re.match(r'^[A-Za-z_]+(?:\s+[A-Za-z_]+)*\s+-?\d+(?:\.\d+)?', part.strip()):
                                matches += 1

                        if matches < len(parts) * 0.5:
                            return False
                    else:
                        return False

                return True

        except Exception:
            return False

    def _parse_sensor_file(self, file_path: str, delimiter: str) -> pd.DataFrame:
        """
        Parser específico para arquivos de sensores no formato KEY value
        """
        data_rows = []
        all_columns = set()

        with open(file_path, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                if not line:
                    continue

                # Dividir por delimitador
                parts = line.split(delimiter)
                row_data = {}

                for part in parts:
                    part = part.strip()
                    if not part:
                        continue

                    # Tentar extrair chave e valor
                    # Padrões possíveis:
                    # 1. "X -9810" (chave única com valor)
                    # 2. "ADC_RAW 2955" (chave com underscore)
                    # 3. "VOLTAGE 2.465V" (valor com unidade)
                    # 4. "ADC MV 2465" (chave com espaço)

                    # Primeiro, tentar padrão mais específico
                    match = re.match(r'^([A-Za-z_]+(?:\s+[A-Za-z_]+)*)\s+(-?\d+(?:\.\d+)?)', part)

                    if match:
                        key = match.group(1).strip().replace(' ', '_').lower()
                        value = float(match.group(2))
                        row_data[key] = value
                        all_columns.add(key)
                    else:
                        # Tentar padrão com unidade (ex: "2.465V")
                        match = re.match(r'^([A-Za-z_]+(?:\s+[A-Za-z_]+)*)\s+(-?\d+(?:\.\d+)?)[A-Za-z]*', part)
                        if match:
                            key = match.group(1).strip().replace(' ', '_').lower()
                            value = float(match.group(2))
                            row_data[key] = value
                            all_columns.add(key)

                if row_data:
                    data_rows.append(row_data)

        if not data_rows:
            raise ValueError("Nenhum dado válido encontrado no arquivo")

        # Criar DataFrame com todas as colunas encontradas
        df = pd.DataFrame(data_rows)

        # Garantir que todas as colunas estão presentes (preencher com NaN se necessário)
        for col in all_columns:
            if col not in df.columns:
                df[col] = np.nan

        # Ordenar colunas para consistência
        df = df[sorted(df.columns)]

        print(f"Arquivo sensor processado: {len(df)} linhas, colunas: {list(df.columns)}")

        return df

    def _parse_txt_file(self, file_path: str, delimiter: str,
                        has_headers: bool) -> pd.DataFrame:
        """
        Parser genérico para arquivos TXT com estruturas variadas
        """
        # Primeiro verificar se é formato sensor
        if self._detect_sensor_format(file_path, delimiter):
            return self._parse_sensor_file(file_path, delimiter)

        # Caso contrário, usar parser genérico
        data_rows = []

        with open(file_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()

            for line in lines:
                line = line.strip()
                if not line:
                    continue

                if delimiter in line:
                    # Formato CSV simples com delimitador
                    parts = [p.strip() for p in line.split(delimiter)]
                    data_rows.append(parts)
                else:
                    # Tentar detectar valores numéricos e extrair
                    numbers = re.findall(r'-?\d+\.?\d*', line)
                    if numbers:
                        data_rows.append(numbers)

        if not data_rows:
            raise ValueError("Nenhum dado válido encontrado no arquivo")

        # Criar DataFrame
        df = pd.DataFrame(data_rows)

        return df

    def add_target_label(self, df: pd.DataFrame, label_value: str,
                         target_column_name: str = 'target') -> pd.DataFrame:
        """
        Adiciona uma coluna target com um valor específico

        Args:
            df: DataFrame original
            label_value: Valor do label (ex: 'ligado', 'defeito')
            target_column_name: Nome da coluna target

        Returns:
            DataFrame com coluna target adicionada
        """
        df_copy = df.copy()
        df_copy[target_column_name] = label_value
        return df_copy

    def merge_datasets(self, dataframes: List[pd.DataFrame]) -> pd.DataFrame:
        """
        Consolida múltiplos DataFrames em um único dataset

        Args:
            dataframes: Lista de DataFrames para consolidar

        Returns:
            DataFrame consolidado
        """
        if not dataframes:
            raise ValueError("Lista de DataFrames está vazia")

        # Para debug
        print(f"Consolidando {len(dataframes)} DataFrames...")

        for i, df in enumerate(dataframes):
            print(f"  DF {i}: shape={df.shape}, columns={list(df.columns)}")

        # Concatenar todos os DataFrames
        # O pandas vai alinhar automaticamente as colunas
        merged_df = pd.concat(dataframes, ignore_index=True, sort=False)

        print(f"DataFrame consolidado: shape={merged_df.shape}, columns={list(merged_df.columns)}")

        return merged_df

    def detect_column_types(self, df: pd.DataFrame) -> Dict[str, str]:
        """
        Detecta automaticamente os tipos de dados de cada coluna

        Args:
            df: DataFrame para análise

        Returns:
            Dicionário com tipos de cada coluna
        """
        column_types = {}

        for col in df.columns:
            # Tentar converter para numérico
            try:
                pd.to_numeric(df[col])
                # Verificar se é inteiro ou float
                if df[col].dtype in ['int64', 'int32']:
                    column_types[col] = 'integer'
                else:
                    column_types[col] = 'float'
            except (ValueError, TypeError):
                # É categórico/string
                unique_ratio = df[col].nunique() / len(df)
                if unique_ratio < 0.5:
                    column_types[col] = 'categorical'
                else:
                    column_types[col] = 'text'

        self.column_types = column_types
        return column_types

    def generate_statistics(self, df: pd.DataFrame) -> Dict:
        """
        Gera estatísticas descritivas do dataset

        Args:
            df: DataFrame para análise

        Returns:
            Dicionário com estatísticas
        """
        stats = {
            'total_rows': int(len(df)),
            'total_columns': int(len(df.columns)),
            'columns': list(df.columns),
            'missing_values': {},
            'column_stats': {}
        }

        # Calcular missing values com tratamento para NaN
        for col in df.columns:
            null_count = df[col].isnull().sum()
            stats['missing_values'][col] = int(null_count) if not pd.isna(null_count) else 0

        # Detectar tipos se ainda não foi feito
        if not self.column_types:
            self.detect_column_types(df)

        # Estatísticas por coluna
        for col in df.columns:
            col_stats = {
                'type': self.column_types.get(col, 'unknown'),
                'non_null_count': int(df[col].count()),
                'null_count': int(df[col].isnull().sum()),
            }

            # Se é numérico, adicionar estatísticas
            if self.column_types.get(col) in ['integer', 'float']:
                try:
                    numeric_col = pd.to_numeric(df[col], errors='coerce')

                    # Calcular estatísticas apenas para valores não-NaN
                    if numeric_col.notna().any():
                        mean_val = numeric_col.mean()
                        std_val = numeric_col.std()
                        min_val = numeric_col.min()
                        max_val = numeric_col.max()
                        median_val = numeric_col.median()

                        # Converter para Python float, tratando NaN
                        col_stats.update({
                            'mean': float(mean_val) if pd.notna(mean_val) else None,
                            'std': float(std_val) if pd.notna(std_val) else None,
                            'min': float(min_val) if pd.notna(min_val) else None,
                            'max': float(max_val) if pd.notna(max_val) else None,
                            'median': float(median_val) if pd.notna(median_val) else None,
                        })
                    else:
                        # Todos os valores são NaN
                        col_stats.update({
                            'mean': None,
                            'std': None,
                            'min': None,
                            'max': None,
                            'median': None,
                        })
                except Exception as e:
                    print(f"Aviso: Erro ao calcular estatísticas da coluna {col}: {e}")
                    col_stats.update({
                        'mean': None,
                        'std': None,
                        'min': None,
                        'max': None,
                        'median': None,
                    })

            # Se é categórico, adicionar contagens
            elif self.column_types.get(col) == 'categorical':
                col_stats['unique_values'] = int(df[col].nunique())
                # Converter value_counts para dict com valores Python nativos
                value_counts = df[col].value_counts().head(10).to_dict()  # Limitar a 10 valores mais comuns
                col_stats['value_counts'] = {str(k): int(v) for k, v in value_counts.items()}

            stats['column_stats'][col] = col_stats

        self.statistics = stats
        return stats

    def detect_outliers(self, df: pd.DataFrame, method: str = 'zscore',
                        threshold: float = 3.0) -> pd.Series:
        """
        Detecta outliers nas colunas numéricas

        Args:
            df: DataFrame para análise
            method: Método de detecção ('zscore' ou 'iqr')
            threshold: Threshold para detecção (padrão: 3.0 para zscore)

        Returns:
            Series booleana indicando outliers
        """
        if not self.column_types:
            self.detect_column_types(df)

        outliers_mask = pd.Series([False] * len(df))
        numeric_cols = [col for col, dtype in self.column_types.items()
                        if dtype in ['integer', 'float']]

        for col in numeric_cols:
            try:
                numeric_col = pd.to_numeric(df[col], errors='coerce')

                # Apenas processar se há valores não-NaN
                if numeric_col.notna().any():
                    if method == 'zscore':
                        z_scores = np.abs(stats.zscore(numeric_col.dropna()))
                        col_outliers = pd.Series([False] * len(df))
                        non_na_indices = numeric_col.notna()
                        col_outliers[non_na_indices] = pd.Series(z_scores > threshold, index=numeric_col.dropna().index)

                    elif method == 'iqr':
                        Q1 = numeric_col.quantile(0.25)
                        Q3 = numeric_col.quantile(0.75)
                        IQR = Q3 - Q1
                        col_outliers = (numeric_col < (Q1 - 1.5 * IQR)) | \
                                       (numeric_col > (Q3 + 1.5 * IQR))

                    outliers_mask |= col_outliers

            except Exception as e:
                print(f"Erro ao detectar outliers na coluna {col}: {e}")
                continue

        return outliers_mask

    def export_consolidated_csv(self, df: pd.DataFrame, output_path: str):
        """
        Exporta DataFrame consolidado para CSV

        Args:
            df: DataFrame para exportar
            output_path: Caminho do arquivo de saída
        """
        # Criar diretório se não existir
        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        # Salvar CSV
        df.to_csv(output_path, index=False, encoding='utf-8')

        print(f"Dataset consolidado salvo em: {output_path}")
        print(f"Shape: {df.shape}")

    def process_datafile(self, file_path: str, delimiter: str = ',',
                         target_label: Optional[str] = None,
                         has_target_column: bool = False) -> Tuple[pd.DataFrame, Dict]:
        """
        Pipeline completo de processamento de um arquivo

        Args:
            file_path: Caminho do arquivo
            delimiter: Delimitador
            target_label: Label para adicionar (se não tem coluna target)
            has_target_column: Se já tem coluna target

        Returns:
            Tuple com (DataFrame processado, Estatísticas)
        """
        # Detectar tipo de arquivo
        file_ext = os.path.splitext(file_path)[1].lower()
        file_type = 'csv' if file_ext == '.csv' else 'txt'

        # Processar arquivo
        df = self.parse_file(file_path, delimiter, file_type)

        # Adicionar target label se necessário
        if not has_target_column and target_label:
            df = self.add_target_label(df, target_label)

        # Gerar estatísticas
        stats = self.generate_statistics(df)

        return df, stats

    def consolidate_project_data(self, file_configs: List[Dict]) -> Tuple[pd.DataFrame, Dict]:
        """
        Consolida múltiplos arquivos de um projeto

        Args:
            file_configs: Lista de dicts com configurações de cada arquivo
                         [{'path': ..., 'delimiter': ..., 'target_label': ...}, ...]

        Returns:
            Tuple com (DataFrame consolidado, Estatísticas)
        """
        dataframes = []

        for config in file_configs:
            df, _ = self.process_datafile(
                file_path=config['path'],
                delimiter=config.get('delimiter', ','),
                target_label=config.get('target_label'),
                has_target_column=config.get('has_target_column', False)
            )
            dataframes.append(df)

        # Consolidar todos os DataFrames
        merged_df = self.merge_datasets(dataframes)

        # Gerar estatísticas finais
        final_stats = self.generate_statistics(merged_df)

        # Detectar outliers
        outliers = self.detect_outliers(merged_df)
        final_stats['outliers_count'] = int(outliers.sum())
        final_stats['outliers_percentage'] = float(outliers.mean() * 100) if len(outliers) > 0 else 0.0

        self.data = merged_df

        return merged_df, final_stats


class GenericCNNTrainer:
    """
    Treinador genérico de modelos CNN 1D para classificação.
    Adaptável a qualquer tipo de dado tabular.
    """

    def __init__(self, model_name: str = "generic_classifier"):
        self.model_name = model_name
        self.model = None
        self.scaler = None
        self.label_encoder = None
        self.history = None
        self.X_train = None
        self.X_test = None
        self.y_train = None
        self.y_test = None
        self.feature_names = None
        self.num_classes = None

    def load_data(self, df: pd.DataFrame, feature_columns: List[str],
                  target_column: str, test_size: float = 0.2,
                  random_state: int = 42) -> Dict:
        """
        Carrega e prepara os dados para treinamento

        Args:
            df: DataFrame com os dados
            feature_columns: Lista de colunas para features
            target_column: Nome da coluna target
            test_size: Proporção para teste
            random_state: Seed para reprodutibilidade

        Returns:
            Dict com informações do carregamento
        """
        print("📊 Carregando dados...")

        # Verificar se as colunas existem
        missing_cols = [col for col in feature_columns if col not in df.columns]
        if missing_cols:
            raise ValueError(f"Colunas não encontradas: {missing_cols}")

        if target_column not in df.columns:
            raise ValueError(f"Coluna target '{target_column}' não encontrada")

        self.feature_names = feature_columns

        # Separar features e target
        X = df[feature_columns].values.astype(np.float32)
        y_raw = df[target_column].values

        # Codificar labels se forem strings
        if y_raw.dtype == object or isinstance(y_raw[0], str):
            self.label_encoder = LabelEncoder()
            y = self.label_encoder.fit_transform(y_raw)
            print(f"   - Classes detectadas: {list(self.label_encoder.classes_)}")
        else:
            y = y_raw

        self.num_classes = len(np.unique(y))

        # Normalizar features
        self.scaler = StandardScaler()
        X_scaled = self.scaler.fit_transform(X)

        # Split train/test
        self.X_train, self.X_test, self.y_train, self.y_test = train_test_split(
            X_scaled, y, test_size=test_size, random_state=random_state, stratify=y
        )

        print(f"✅ Dados carregados:")
        print(f"   - Features: {len(feature_columns)} ({feature_columns})")
        print(f"   - Samples total: {len(X)}")
        print(f"   - Train: {len(self.X_train)} | Test: {len(self.X_test)}")
        print(f"   - Classes: {self.num_classes}")

        return {
            'total_samples': len(X),
            'train_samples': len(self.X_train),
            'test_samples': len(self.X_test),
            'num_features': len(feature_columns),
            'num_classes': self.num_classes,
            'feature_names': feature_columns
        }

    def build_model(self, input_shape: Optional[int] = None,
                    conv_filters: List[int] = [32, 64],
                    dense_units: int = 50,
                    dropout_rate: float = 0.3) -> keras.Model:
        """
        Constrói o modelo CNN 1D de forma genérica

        Args:
            input_shape: Número de features de entrada
            conv_filters: Número de filtros para cada camada Conv1D
            dense_units: Unidades na camada dense
            dropout_rate: Taxa de dropout

        Returns:
            Modelo compilado
        """
        print("🗿 Construindo modelo CNN 1D...")

        if input_shape is None:
            input_shape = self.X_train.shape[1]

        if self.num_classes is None:
            raise ValueError("Execute load_data() primeiro")

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
            layers.Dense(self.num_classes, activation='softmax')
        ])

        # Compilar modelo
        self.model.compile(
            optimizer='adam',
            loss='sparse_categorical_crossentropy',
            metrics=['accuracy']
        )

        print("✅ Modelo construído com sucesso!")

        return self.model

    def train_model(self, epochs: int = 100, batch_size: int = 32,
                    validation_split: float = 0.2,
                    early_stopping: bool = True,
                    patience: int = 10,
                    verbose: int = 1) -> Dict:
        """
        Treina o modelo

        Args:
            epochs: Número máximo de épocas
            batch_size: Tamanho do batch
            validation_split: Proporção para validação
            early_stopping: Usar early stopping
            patience: Paciência para early stopping
            verbose: Verbosidade do treinamento

        Returns:
            Dict com informações do treinamento
        """
        print("🚀 Iniciando treinamento...")

        if self.model is None:
            raise ValueError("Execute build_model() primeiro")

        callbacks = []

        if early_stopping:
            early_stop = keras.callbacks.EarlyStopping(
                monitor='val_loss',
                patience=patience,
                restore_best_weights=True,
                verbose=1
            )
            callbacks.append(early_stop)

        # Treinar modelo
        start_time = datetime.now()

        self.history = self.model.fit(
            self.X_train, self.y_train,
            epochs=epochs,
            batch_size=batch_size,
            validation_split=validation_split,
            callbacks=callbacks,
            verbose=verbose
        )

        training_time = (datetime.now() - start_time).total_seconds()

        print(f"✅ Treinamento concluído em {training_time:.2f} segundos!")

        return {
            'training_time_seconds': training_time,
            'epochs_completed': len(self.history.history['loss']),
            'final_train_accuracy': float(self.history.history['accuracy'][-1]),
            'final_val_accuracy': float(self.history.history['val_accuracy'][-1])
        }

    def evaluate_model(self) -> Dict:
        """
        Avalia o modelo nos dados de teste

        Returns:
            Dict com métricas de avaliação
        """
        print("📈 Avaliando modelo...")

        if self.model is None:
            raise ValueError("Modelo não foi treinado")

        # Predições
        y_pred = self.model.predict(self.X_test, verbose=0)
        y_pred_classes = np.argmax(y_pred, axis=1)

        # Métricas
        test_loss, test_accuracy = self.model.evaluate(self.X_test, self.y_test, verbose=0)

        # Matriz de confusão
        cm = confusion_matrix(self.y_test, y_pred_classes)

        # Relatório de classificação
        if self.label_encoder:
            target_names = list(self.label_encoder.classes_)
        else:
            target_names = [str(i) for i in range(self.num_classes)]

        cr = classification_report(self.y_test, y_pred_classes,
                                   target_names=target_names,
                                   output_dict=True)

        print(f"✅ Resultados no teste:")
        print(f"   - Acurácia: {test_accuracy:.4f}")
        print(f"   - Loss: {test_loss:.4f}")

        return {
            'accuracy': float(test_accuracy),
            'loss': float(test_loss),
            'confusion_matrix': cm.tolist(),
            'classification_report': cr,
            'predictions': y_pred_classes.tolist()
        }

    def save_model(self, save_dir: str) -> Dict[str, str]:
        """
        Salva o modelo e artefatos relacionados

        Args:
            save_dir: Diretório para salvar os arquivos

        Returns:
            Dict com caminhos dos arquivos salvos
        """
        print("💾 Salvando modelo...")

        os.makedirs(save_dir, exist_ok=True)

        paths = {}

        # Salvar modelo Keras
        keras_path = os.path.join(save_dir, f'{self.model_name}.h5')
        self.model.save(keras_path)
        paths['keras_model'] = keras_path

        # Salvar scaler
        scaler_path = os.path.join(save_dir, f'{self.model_name}_scaler.pkl')
        joblib.dump(self.scaler, scaler_path)
        paths['scaler'] = scaler_path

        # Salvar label encoder se existir
        if self.label_encoder:
            encoder_path = os.path.join(save_dir, f'{self.model_name}_encoder.pkl')
            joblib.dump(self.label_encoder, encoder_path)
            paths['label_encoder'] = encoder_path

        print(f"✅ Modelo salvo em: {save_dir}")

        return paths

    def convert_to_tflite(self, save_dir: str, quantize: bool = True) -> Tuple[str, float]:
        """
        Converte modelo para TensorFlow Lite

        Args:
            save_dir: Diretório para salvar
            quantize: Aplicar quantização int8

        Returns:
            Tuple (caminho do arquivo, tamanho em KB)
        """
        print("🔄 Convertendo para TensorFlow Lite...")

        if self.model is None:
            raise ValueError("Modelo não foi treinado")

        # Converter
        converter = tf.lite.TFLiteConverter.from_keras_model(self.model)

        if quantize:
            # Otimizações para dispositivos embarcados
            converter.optimizations = [tf.lite.Optimize.DEFAULT]

            # Dataset representativo para quantização
            def representative_data_gen():
                for i in range(min(100, len(self.X_test))):
                    yield [self.X_test[i:i + 1].astype(np.float32)]

            converter.representative_dataset = representative_data_gen
            converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
            converter.inference_input_type = tf.int8
            converter.inference_output_type = tf.int8

        tflite_model = converter.convert()

        # Salvar modelo TFLite
        os.makedirs(save_dir, exist_ok=True)
        tflite_path = os.path.join(save_dir, f'{self.model_name}.tflite')

        with open(tflite_path, 'wb') as f:
            f.write(tflite_model)

        # Calcular tamanho
        size_kb = len(tflite_model) / 1024

        print(f"✅ Modelo TFLite salvo: {tflite_path}")
        print(f"   - Tamanho: {size_kb:.2f} KB")
        print(f"   - Quantizado: {'Sim' if quantize else 'Não'}")

        return tflite_path, size_kb

    def train_complete_pipeline(self, df: pd.DataFrame,
                                feature_columns: List[str],
                                target_column: str,
                                save_dir: str,
                                epochs: int = 100,
                                batch_size: int = 32,
                                test_size: float = 0.2,
                                convert_tflite: bool = True) -> Dict:
        """
        Pipeline completo de treinamento

        Args:
            df: DataFrame com dados
            feature_columns: Colunas para features
            target_column: Coluna target
            save_dir: Diretório para salvar modelos
            epochs: Épocas de treinamento
            batch_size: Tamanho do batch
            test_size: Proporção para teste
            convert_tflite: Converter para TFLite

        Returns:
            Dict com todos os resultados
        """
        print("🎯 INICIANDO PIPELINE COMPLETO DE TREINAMENTO")
        print("=" * 60)

        try:
            # 1. Carregar dados
            load_info = self.load_data(df, feature_columns, target_column, test_size)

            # 2. Construir modelo
            self.build_model()

            # 3. Treinar
            train_info = self.train_model(epochs=epochs, batch_size=batch_size)

            # 4. Avaliar
            eval_info = self.evaluate_model()

            # 5. Salvar modelo
            save_paths = self.save_model(save_dir)

            # 6. Converter para TFLite
            tflite_path = None
            model_size_kb = None

            if convert_tflite:
                tflite_path, model_size_kb = self.convert_to_tflite(save_dir)
                save_paths['tflite_model'] = tflite_path

            print("=" * 60)
            print("🎉 PIPELINE CONCLUÍDO COM SUCESSO!")

            # Consolidar resultados
            results = {
                'model': self.model,
                'scaler': self.scaler,
                'label_encoder': self.label_encoder,
                'accuracy': eval_info['accuracy'],
                'loss': eval_info['loss'],
                'confusion_matrix': eval_info['confusion_matrix'],
                'classification_report': eval_info['classification_report'],
                'training_history': {
                    'loss': [float(x) for x in self.history.history['loss']],
                    'accuracy': [float(x) for x in self.history.history['accuracy']],
                    'val_loss': [float(x) for x in self.history.history['val_loss']],
                    'val_accuracy': [float(x) for x in self.history.history['val_accuracy']],
                },
                'training_time_seconds': train_info['training_time_seconds'],
                'model_size_kb': model_size_kb,
                'file_paths': save_paths,
                'feature_names': feature_columns,
                'num_classes': self.num_classes,
                'load_info': load_info
            }

            return results

        except Exception as e:
            print(f"❌ Erro durante o treinamento: {str(e)}")
            raise e