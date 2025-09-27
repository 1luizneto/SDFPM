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
│   │   └── motor_data_raw.csv
│   ├── txt_files/         # Arquivos brutos de dados do sensor
│   │   ├── motor_ligado_26_09.txt
│   │   ├── motor_desligado_26_09.txt
│   │   └── motor_com_falha_26_09.txt
│   └── images/            # Gráficos gerados pela análise
│       ├── motor_analysis.png
│       └── motor_time_series.png
├── utils/
│   └── data_analysys.py   # Script de processamento e análise de dados
└── requirements.txt       # Dependências do projeto
```

## Sobre o Processador de Dados

O arquivo `data_analysys.py` contém a implementação do `MotorDataProcessor`, uma classe responsável por:

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

## Executando a Análise de Dados

Com o ambiente virtual ativado, execute o script de análise:

```bash
# No Linux
python utils/data_analysys.py

# No Windows
python utils\data_analysys.py
```

O script irá:
1. Carregar os arquivos de texto com os dados do sensor
2. Analisar estatisticamente os dados
3. Detectar possíveis outliers
4. Criar features adicionais para análise e modelagem
5. Gerar gráficos e visualizações na pasta `data/images/`
6. Salvar os dados processados em CSVs na pasta `data/csv_files/`

## Saídas Geradas

- **CSVs**: 
  - `motor_data_raw.csv`: Dados brutos organizados
  - `motor_data_processed.csv`: Dados com features adicionais

- **Visualizações**:
  - `motor_analysis.png`: Análise comparativa entre estados do motor
  - `motor_time_series.png`: Séries temporais das vibrações por eixo

## Personalização

Para analisar novos conjuntos de dados, adicione os arquivos TXT na pasta `data/txt_files/` e atualize as configurações no método `main()` do script. 