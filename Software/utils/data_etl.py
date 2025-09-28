import pandas as pd

df = pd.read_csv(r"C:\Users\Raul\SDFPM\Software\data\csv_files\motor_data_raw.csv")

df = df.drop('timestamp', axis=1)



df['status'] = df['status'].map({'ligado': 1, 'desligado': 0, 'defeito': 2})
print(df.head())
print(df['status'].value_counts())

df.to_csv(r'C:\Users\Raul\SDFPM\SOFTWARE\data\csv_files\motor_data_training.csv', index=False)