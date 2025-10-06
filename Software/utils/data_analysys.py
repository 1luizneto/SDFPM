import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import re
import os
from pathlib import Path
from scipy import stats

class MotorDataProcessor:
    def __init__(self):
        self.data = None
        
        # Configurar estrutura de pastas
        self.folder = Path(__file__).parent.parent
        self.images_folder = self.folder / "data" / "images"
        
        # Criar pasta de imagens se não existir
        self.images_folder.mkdir(parents=True, exist_ok=True)
        print(f"Pasta de imagens configurada em: {self.images_folder}")
        
    def parse_sensor_line(self, line):
        """
        Processa uma linha do arquivo de dados do sensor
        Formato: X value;Y value;Z value;ADC_RAW value;ADC_MV value;VOLTAGE value
        """
        if not line.strip():
            return None
            
        try:
            # Extrair valores X, Y, Z, ADC_RAW usando regex
            x_match = re.search(r'X (-?\d+)', line)
            y_match = re.search(r'Y (-?\d+)', line)
            z_match = re.search(r'Z (-?\d+)', line)
            adc_raw_match = re.search(r'ADC_RAW (\d+)', line)

            if not all([x_match, y_match, z_match, adc_raw_match]):
                return None
                
            return {
                'x': int(x_match.group(1)),
                'y': int(y_match.group(1)),
                'z': int(z_match.group(1)),
                'adc_raw': int(adc_raw_match.group(1))
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
            
            # ADC
            print(f"\nADC_RAW:")
            print(f"  Média: {subset['adc_raw'].mean():.2f}")
            print(f"  Desvio: {subset['adc_raw'].std():.2f}")
            print(f"  Min: {subset['adc_raw'].min()}")
            print(f"  Max: {subset['adc_raw'].max()}")

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
    
    def save_to_csv(self, filename="motor_data.csv"):
        """
        Salva os dados em CSV
        """
        if self.data is None:
            print("Nenhum dado para salvar!")
            return
        
        self.data.to_csv(filename, index=False)
        print(f"Dados salvos em: {filename}")
        print(f"Shape: {self.data.shape}")
        print(f"Colunas: {list(self.data.columns)}")
    
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
                                    labels=self.data['status'].unique(),
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
        
        # ADC por status
        for i, status in enumerate(self.data['status'].unique()):
            subset = self.data[self.data['status'] == status]
            axes[1, 2].hist(subset['adc_raw'], alpha=0.7, label=status, bins=30,
                           color=colors[i % len(colors)])
        
        axes[1, 2].set_title('Distribuição ADC_RAW')
        axes[1, 2].set_xlabel('ADC_RAW')
        axes[1, 2].set_ylabel('Frequência')
        axes[1, 2].legend()
        axes[1, 2].grid(True, alpha=0.3)
        
        plt.tight_layout()
        
        if save_plots:
            caminho_to_save = self.images_folder / 'motor_analysis.png'
            plt.savefig(caminho_to_save, dpi=300, bbox_inches='tight')
            print(f"Gráfico salvo como: {caminho_to_save}")
        
        plt.show()

def main():
    # Inicializar o processador
    folder = Path(__file__).parent.parent 
    print(f"Pasta raiz do projeto (assumida): {folder}")
    processor = MotorDataProcessor()
    
    # Configurar arquivos (ajuste os caminhos conforme sua estrutura de pastas)
    file_configs = [
        (folder / "data/txt_files/Teste com o motor ligado 03.10.txt", "ligado"),
        (folder / "data/txt_files/teste com o motor desligado 03.10.txt", "desligado"),
        (folder / "data/txt_files/Teste com o motor com falha 03.10.txt", "defeito")
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
        
        print("\n4. Salvando em CSV...")
        processor.save_to_csv(str(folder / "data/csv_files/motor_data_training.csv"))
        
        print("\n5. Gerando e salvando gráficos...")
        processor.plot_data_analysis(save_plots=True)
        
        print("\n\n=== RESUMO ===")
        print(f"Total de amostras: {len(data)}")
        print(f"Colunas: {list(data.columns)}")
        print("Arquivo CSV e gráficos salvos com sucesso!")
        
    else:
        print("\nNenhum dado foi carregado. Verifique os caminhos e o conteúdo dos arquivos de entrada.")

if __name__ == "__main__":
    main()
