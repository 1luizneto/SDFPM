import pandas as pd
from pathlib import Path

FOLDER = Path(__file__).parent.parent / 'data' / 'csv_files'

df = pd.read_csv(FOLDER / 'motor_data_training.csv')

# Imprime os nomes das colunas para verificação
print(df.columns)

df['status'] = df['status'].map({'ligado': 1, 'desligado': 0, 'defeito': 2})
print(df.head())
print(df['status'].value_counts())

df.to_csv(FOLDER / 'motor_data_training_numerical.csv', index=False)