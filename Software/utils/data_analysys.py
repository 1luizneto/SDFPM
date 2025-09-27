import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import re
import os
from pathlib import Path
from scipy import stats
from pathlib import Path

class MotorDataProcessor:
    def __init__(self):
        self.data = None
        self.processed_data = None
        
        # Configurar estrutura de pastas
        self.folder = Path(__file__).parent.parent
        self.images_folder = self.folder / "data" / "images"
        
        # Criar pasta de imagens se não existir
        self.images_folder.mkdir(parents=True, exist_ok=True)
        print(f"Pasta de imagens configurada em: {self.images_folder}")
        
    def parse_sensor_line(self, line):
        """
        Processa uma linha do arquivo de dados do sensor
        Formato: timestamp -> X value;Y value;Z value
        """
        if not line.strip() or '->' not in line:
            return None
            
        try:
            parts = line.split('->')
            if len(parts) != 2:
                return None
                
            timestamp = parts[0].strip()
            coords = parts[1].strip()
            
            # Extrair valores X, Y, Z usando regex
            x_match = re.search(r'X (-?\d+)', coords)
            y_match = re.search(r'Y (-?\d+)', coords)
            z_match = re.search(r'Z (-?\d+)', coords)
            
            if not all([x_match, y_match, z_match]):
                return None
                
            return {
                'timestamp': timestamp,
                'x': int(x_match.group(1)),
                'y': int(y_match.group(1)),
                'z': int(z_match.group(1))
            }
        except:
            return None
    
    def load_txt_file(self, filepath, status_label):
        """
        Carrega arquivo .txt e converte para lista de dicionários
        """
        data_list = []
        
        try:
            with open(filepath, 'r', encoding='utf-8') as file:
                for line in file:
                    parsed_line = self.parse_sensor_line(line)
                    if parsed_line:
                        parsed_line['status'] = status_label
                        data_list.append(parsed_line)
            
            print(f"Arquivo {filepath}: {len(data_list)} amostras carregadas")
            return data_list
            
        except FileNotFoundError:
            print(f"Arquivo {filepath} não encontrado!")
            return []
        except Exception as e:
            print(f"Erro ao processar arquivo {filepath}: {e}")
            return []
    
    def load_multiple_files(self, file_configs):
        """
        Carrega múltiplos arquivos com seus respectivos labels
        file_configs: lista de tuplas [(filepath, status_label), ...]
        """
        all_data = []
        
        for filepath, status_label in file_configs:
            file_data = self.load_txt_file(filepath, status_label)
            all_data.extend(file_data)
        
        if all_data:
            self.data = pd.DataFrame(all_data)
            print(f"\nTotal de amostras carregadas: {len(self.data)}")
            print(f"Distribuição por status: \n{self.data['status'].value_counts()}")
        
        return self.data
    
    def create_features(self):
        """
        Cria features adicionais a partir dos dados brutos
        """
        if self.data is None:
            print("Nenhum dado carregado!")
            return None
        
        df = self.data.copy()
        
        # Magnitude do vetor (vibração total)
        df['magnitude'] = np.sqrt(df['x']**2 + df['y']**2 + df['z']**2)
        
        # Features estatísticas por janela temporal
        # Convertendo timestamp para datetime (assumindo formato HH:MM:SS:mmm)
        try:
            df['time_parsed'] = pd.to_datetime(df['timestamp'], format='%H:%M:%S:%f')
        except:
            # Se não conseguir converter, criar índice sequencial
            df['time_parsed'] = pd.to_datetime(range(len(df)), unit='ms')
        
        # Ordenar por tempo
        df = df.sort_values('time_parsed')
        
        # Features de janela móvel (últimas N amostras)
        window_size = 10
        
        for axis in ['x', 'y', 'z', 'magnitude']:
            # Média móvel
            df[f'{axis}_mean'] = df[axis].rolling(window=window_size, min_periods=1).mean()
            
            # Desvio padrão móvel
            df[f'{axis}_std'] = df[axis].rolling(window=window_size, min_periods=1).std()
            
            # Valor máximo na janela
            df[f'{axis}_max'] = df[axis].rolling(window=window_size, min_periods=1).max()
            
            # Valor mínimo na janela
            df[f'{axis}_min'] = df[axis].rolling(window=window_size, min_periods=1).min()
            
            # Range na janela
            df[f'{axis}_range'] = df[f'{axis}_max'] - df[f'{axis}_min']
        
        # Features de frequência (FFT básica)
        # Para cada janela, calcular componentes dominantes
        
        self.processed_data = df
        return df
    
    def analyze_data(self):
        """
        Realiza análise estatística dos dados
        """
        if self.data is None:
            print("Nenhum dado carregado!")
            return
        
        print("\n=== ANÁLISE ESTATÍSTICA DOS DADOS ===")
        
        # Estatísticas por status
        for status in self.data['status'].unique():
            subset = self.data[self.data['status'] == status]
            
            print(f"\n--- {status.upper()} ({len(subset)} amostras) ---")
            
            for axis in ['x', 'y', 'z']:
                values = subset[axis]
                print(f"\nEixo {axis.upper()}:")
                print(f"  Média: {values.mean():.2f}")
                print(f"  Desvio: {values.std():.2f}")
                print(f"  Min: {values.min()}")
                print(f"  Max: {values.max()}")
                print(f"  Range: {values.max() - values.min()}")
            
            # Magnitude
            magnitude = np.sqrt(subset['x']**2 + subset['y']**2 + subset['z']**2)
            print(f"\nMagnitude:")
            print(f"  Média: {magnitude.mean():.2f}")
            print(f"  Desvio: {magnitude.std():.2f}")
            print(f"  Min: {magnitude.min():.2f}")
            print(f"  Max: {magnitude.max():.2f}")
    
    def detect_outliers(self, method='zscore', threshold=3):
        """
        Detecta outliers nos dados
        """
        if self.data is None:
            print("Nenhum dado carregado!")
            return None
        
        df = self.data.copy()
        outliers_mask = pd.Series([False] * len(df))
        
        for axis in ['x', 'y', 'z']:
            if method == 'zscore':
                z_scores = np.abs(stats.zscore(df[axis]))
                axis_outliers = z_scores > threshold
            elif method == 'iqr':
                Q1 = df[axis].quantile(0.25)
                Q3 = df[axis].quantile(0.75)
                IQR = Q3 - Q1
                axis_outliers = (df[axis] < (Q1 - 1.5 * IQR)) | (df[axis] > (Q3 + 1.5 * IQR))
            
            outliers_mask |= axis_outliers
        
        print(f"\nOutliers detectados: {outliers_mask.sum()} ({outliers_mask.mean()*100:.1f}%)")
        
        return outliers_mask
    
    def save_to_csv(self, filename="motor_data.csv", include_features=True):
        """
        Salva os dados processados em CSV
        """
        if include_features and self.processed_data is not None:
            data_to_save = self.processed_data
        elif self.data is not None:
            data_to_save = self.data
        else:
            print("Nenhum dado para salvar!")
            return
        
        data_to_save.to_csv(filename, index=False)
        print(f"Dados salvos em: {filename}")
        print(f"Shape: {data_to_save.shape}")
        print(f"Colunas: {list(data_to_save.columns)}")
    
    def plot_data_analysis(self, save_plots=False):
        """
        Gera gráficos para análise dos dados
        save_plots: Se True, salva os gráficos como imagens além de mostrar
        """
        if self.data is None:
            print("Nenhum dado carregado!")
            return
            
        fig, axes = plt.subplots(2, 3, figsize=(15, 10))
        fig.suptitle('Análise dos Dados do Motor', fontsize=16, fontweight='bold')
        
        # Box plots por eixo
        for i, axis in enumerate(['x', 'y', 'z']):
            data_by_status = [self.data[self.data['status'] == status][axis] 
                              for status in self.data['status'].unique()]
            bp = axes[0, i].boxplot(data_by_status, 
                                    tick_labels=self.data['status'].unique(),
                                    patch_artist=True)
            
            # Colorir as caixas
            colors = ['lightblue', 'lightgreen', 'salmon']
            for patch, color in zip(bp['boxes'], colors[:len(bp['boxes'])]):
                patch.set_facecolor(color)
                
            axes[0, i].set_title(f'Distribuição Eixo {axis.upper()}')
            axes[0, i].set_ylabel('Valores do Sensor')
            axes[0, i].grid(True, alpha=0.3)
        
        # Histogramas da magnitude por status
        colors = ['blue', 'green', 'red']
        for i, status in enumerate(self.data['status'].unique()):
            subset = self.data[self.data['status'] == status]
            magnitude = np.sqrt(subset['x']**2 + subset['y']**2 + subset['z']**2)
            axes[1, 0].hist(magnitude, alpha=0.7, label=status, bins=30, 
                            color=colors[i % len(colors)])
        
        axes[1, 0].set_title('Distribuição da Magnitude')
        axes[1, 0].set_xlabel('Magnitude da Vibração')
        axes[1, 0].set_ylabel('Frequência')
        axes[1, 0].legend()
        axes[1, 0].grid(True, alpha=0.3)
        
        # Scatter plot 3D projetado em 2D
        colors_scatter = ['blue', 'green', 'red']
        for i, status in enumerate(self.data['status'].unique()):
            subset = self.data[self.data['status'] == status]
            axes[1, 1].scatter(subset['x'], subset['y'], alpha=0.6, 
                               label=status, color=colors_scatter[i % len(colors_scatter)])
        
        axes[1, 1].set_title('Projeção X vs Y')
        axes[1, 1].set_xlabel('Eixo X')
        axes[1, 1].set_ylabel('Eixo Y')
        axes[1, 1].legend()
        axes[1, 1].grid(True, alpha=0.3)
        
        # Série temporal da magnitude (calculando aqui mesmo)
        magnitude_series = np.sqrt(self.data['x']**2 + self.data['y']**2 + self.data['z']**2)
        
        # Mostrar primeiras amostras de cada status se disponível
        sample_size = min(100, len(self.data))
        sample_magnitude = magnitude_series.head(sample_size)
        
        axes[1, 2].plot(range(len(sample_magnitude)), sample_magnitude, 
                        'b-', linewidth=1.5, alpha=0.8)
        axes[1, 2].set_title(f'Série Temporal da Magnitude (Primeiras {sample_size} amostras)')
        axes[1, 2].set_xlabel('Índice da Amostra')
        axes[1, 2].set_ylabel('Magnitude')
        axes[1, 2].grid(True, alpha=0.3)
        
        if save_plots:
            caminho_to_save = self.images_folder / 'motor_analysis.png'
            plt.savefig(caminho_to_save, dpi=300, bbox_inches='tight')
            print(f"Gráfico salvo como: {caminho_to_save}")
        
        # Criar gráfico adicional para séries temporais por status
        self._plot_time_series_by_status(save_plots)
    
    def _plot_time_series_by_status(self, save_plots=False):
        """
        Cria gráfico separado para séries temporais por status
        """
        fig, axes = plt.subplots(2, 2, figsize=(15, 11))
        fig.suptitle('Séries Temporais por Status do Motor', fontsize=14, fontweight='bold')
        
        colors = {'ligado': 'blue', 'desligado': 'green', 'defeito': 'red'}
        
        for i, axis in enumerate(['x', 'y', 'z']):
            ax = axes[i//2, i%2] if i < 3 else axes[1, 1]
            
            for status in self.data['status'].unique():
                subset = self.data[self.data['status'] == status]
                sample_size = min(30, len(subset))  # Primeiras 30 amostras
                subset_sample = subset.head(sample_size)
                
                ax.plot(range(len(subset_sample)), subset_sample[axis], 
                        label=status, alpha=0.8, linewidth=1.5,
                        color=colors.get(status, 'black'))
            
            ax.set_title(f'Eixo {axis.upper()}')
            ax.set_xlabel('Amostra')
            ax.set_ylabel(f'Valor {axis.upper()}')
            ax.legend()
            ax.grid(True, alpha=0.3)
            
        for status in self.data['status'].unique():
            subset = self.data[self.data['status'] == status]
            sample_size = min(30, len(subset))
            subset_sample = subset.head(sample_size)
            magnitude = np.sqrt(subset_sample['x']**2 + subset_sample['y']**2 + subset_sample['z']**2)
            
            axes[1, 1].plot(range(len(magnitude)), magnitude, 
                            label=status, alpha=0.8, linewidth=2,
                            color=colors.get(status, 'black'))
        
        axes[1, 1].set_title('Magnitude')
        axes[1, 1].set_xlabel('Amostra')
        axes[1, 1].set_ylabel('Magnitude')
        axes[1, 1].legend()
        axes[1, 1].grid(True, alpha=0.3)
        
        if save_plots:
            caminho_to_save = self.images_folder / 'motor_time_series.png'
            plt.savefig(caminho_to_save, dpi=300, bbox_inches='tight')
            print(f"Gráfico de séries temporais salvo como: {caminho_to_save}")
def main():
    # Inicializar o processador
    folder = Path(__file__).parent.parent 
    print(f"Pasta raiz do projeto (assumida): {folder}")
    processor = MotorDataProcessor()
    
    # Configurar arquivos (ajuste os caminhos conforme sua estrutura de pastas)
    file_configs = [
        (folder / "data/txt_files/motor_ligado_26_09.txt", "ligado"),
        (folder / "data/txt_files/motor_desligado_26_09.txt", "desligado"),
        (folder / "data/txt_files/motor_com_falha_26_09.txt", "defeito")
    ]
    
    print("\n=== PROCESSADOR DE DADOS DO MOTOR ===")
    print("1. Carregando arquivos...")
    
    # Carregar dados
    data = processor.load_multiple_files(file_configs)
    
    if data is not None and not data.empty:
        print("\n2. Analisando dados...")
        processor.analyze_data()
        
        print("\n3. Detectando outliers...")
        outliers = processor.detect_outliers()
        
        print("\n4. Criando features...")
        processed_data = processor.create_features()
        
        print("\n5. Salvando em CSV...")
        processor.save_to_csv(str(processor.folder / "data/csv_files/motor_data_processed.csv"), include_features=True)
        processor.save_to_csv(str(processor.folder / "data/csv_files/motor_data_raw.csv"), include_features=False)
        
        print("\n6. Gerando e salvando gráficos...")
        processor.plot_data_analysis(save_plots=True)
        
        print("\n\n=== RESUMO ===")
        print(f"Total de amostras: {len(data)}")
        print(f"Features criadas: {processed_data.shape[1] if processed_data is not None else 'N/A'}")
        print("Arquivos CSV e gráficos salvos com sucesso!")
        
    else:
        print("\nNenhum dado foi carregado. Verifique os caminhos e o conteúdo dos arquivos de entrada.")

if __name__ == "__main__":
    main()