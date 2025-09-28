# SDFPM
Sistemas de Detecção de Falhas Preditiva de Motor DC

# Software

## Estrutura de Pastas

O projeto está organizado com a seguinte estrutura:

```
SDFPM/
├── data/
│   ├── csv_files/         # Arquivos CSV gerados pelo processamento
│   │   ├── motor_data_processed.csv
│   │   ├── motor_data_raw.csv
│   │   └── motor_data_training.csv
│   ├── txt_files/         # Arquivos brutos de dados do sensor
│   │   ├── motor_ligado_26_09.txt
│   │   ├── motor_desligado_26_09.txt
│   │   └── motor_com_falha_26_09.txt
│   └── images/            # Gráficos gerados pela análise
│       ├── motor_analysis.png
│       └── motor_time_series.png
├── models/                # Modelos de machine learning treinados
├── utils/
│   ├── data_analysys.py   # Script de processamento e análise de dados
│   ├── data_etl.py        # Script para transformação dos dados
│   └── model_training.py  # Script de treinamento de modelo de ML
├── .gitignore             # Configuração de arquivos ignorados pelo Git
├── requirements.txt       # Dependências do projeto
└── README.md              # Documentação do projeto
```

## Processamento e Análise de Dados

### Processador de Dados (data_analysys.py)

O arquivo `data_analysys.py` implementa o `MotorDataProcessor`, uma classe responsável por:

1. Carregar dados brutos de vibração do motor DC
2. Processar e transformar os dados
3. Gerar features adicionais baseadas nos valores X, Y, Z do acelerômetro
4. Criar visualizações e análises estatísticas
5. Detectar outliers e anomalias
6. Salvar dados processados em formato CSV para treinamento de modelos

O script analisa três estados do motor:
- **Ligado**: Funcionamento normal
- **Desligado**: Motor sem operação
- **Defeito**: Motor com falhas simuladas

### ETL de Dados (data_etl.py)

O arquivo `data_etl.py` realiza a transformação dos dados para o formato adequado para treinamento:

1. Carrega os dados brutos processados (`motor_data_raw.csv`)
2. Remove colunas desnecessárias (como timestamp)
3. Converte categorias de status para valores numéricos:
   - Ligado: 1
   - Desligado: 0
   - Defeito: 2
4. Salva os dados transformados para treinamento (`motor_data_training.csv`)

## Modelagem de Machine Learning

### Treinamento do Modelo (model_training.py)

O arquivo `model_training.py` implementa o `MotorCNNTrainer`, uma classe para treinamento de modelos CNN 1D para classificar o estado do motor:

1. Carrega os dados de treinamento
2. Realiza pré-processamento e normalização
3. Constrói um modelo CNN 1D usando TensorFlow/Keras
4. Treina o modelo com validação cruzada
5. Avalia o desempenho do modelo
6. Gera visualizações do processo de treinamento
7. Exporta o modelo para formatos H5 e TFLite (para ESP32)

Os modelos treinados são armazenados na pasta `models/`, incluindo:
- Modelo em formato H5
- Modelo quantizado em formato TFLite para ESP32
- Arquivo de escala para normalização
- Informações sobre o modelo e features utilizadas

## Instalação e Execução

### Pré-requisitos
- Python 3.8 ou superior
- pip (gerenciador de pacotes)

### Instalação no Linux

1. Clone o repositório e navegue até a pasta do projeto:
```bash
git clone git@github.com:1luizneto/SDFPM.git
cd SDFPM
```

2. Crie um ambiente virtual:
```bash
python3 -m venv venv
```

3. Ative o ambiente virtual:
```bash
source venv/bin/activate
```

4. Instale as dependências:
```bash
pip install -r requirements.txt
```

### Instalação no Windows

1. Clone o repositório e navegue até a pasta do projeto:
```bash
git clone git@github.com:1luizneto/SDFPM.git
cd SDFPM
```

2. Crie um ambiente virtual:
```bash
python -m venv venv
```

3. Ative o ambiente virtual:
```bash
venv\Scripts\activate
```

4. Instale as dependências:
```bash
pip install -r requirements.txt
```

## Executando os Scripts

Com o ambiente virtual ativado, execute os scripts na seguinte ordem:

### 1. Processamento de Dados

```bash
# No Linux
python utils/data_analysys.py

# No Windows
python utils\data_analysys.py
```

Este script irá:
- Carregar os arquivos de texto com os dados do sensor
- Analisar estatisticamente os dados
- Detectar possíveis outliers
- Criar features adicionais para análise e modelagem
- Gerar gráficos e visualizações na pasta `data/images/`
- Salvar os dados processados em CSVs na pasta `data/csv_files/`

### 2. Transformação de Dados

```bash
# No Linux
python utils/data_etl.py

# No Windows
python utils\data_etl.py
```

Este script transforma os dados brutos para o formato adequado para treinamento.

### 3. Treinamento do Modelo

```bash
# No Linux
python utils/model_training.py

# No Windows
python utils\model_training.py
```

Este script treina o modelo CNN 1D e exporta os artefatos resultantes para a pasta `models/`.

## Saídas Geradas

- **CSVs**: 
  - `motor_data_raw.csv`: Dados brutos organizados
  - `motor_data_processed.csv`: Dados com features adicionais
  - `motor_data_training.csv`: Dados preparados para treinamento

- **Visualizações**:
  - `motor_analysis.png`: Análise comparativa entre estados do motor
  - `motor_time_series.png`: Séries temporais das vibrações por eixo
  - `sdfpm_motor_v1_training.png`: Gráficos de acurácia e loss do treinamento

- **Modelos**:
  - Modelos treinados e artefatos relacionados na pasta `models/`

## Personalização

Para analisar novos conjuntos de dados, adicione os arquivos TXT na pasta `data/txt_files/` e atualize as configurações nos respectivos scripts conforme necessário.
