# 🧪 Exemplos de Teste da NexusML API

## 📋 Collection de Requisições HTTP

### Base URL
```
http://127.0.0.1:8000/api
```

---

## 1️⃣ PROJETOS

### 1.1 Criar Projeto
```http
POST http://127.0.0.1:8000/api/projects/
Content-Type: application/json

{
    "name": "Motor Classifier",
    "description": "Classificação de estados do motor usando acelerômetro"
}
```

### 1.2 Listar Projetos
```http
GET http://127.0.0.1:8000/api/projects/
```

### 1.3 Obter Projeto Específico
```http
GET http://127.0.0.1:8000/api/projects/{project_id}/
```

### 1.4 Atualizar Projeto
```http
PATCH http://127.0.0.1:8000/api/projects/{project_id}/
Content-Type: application/json

{
    "description": "Nova descrição do projeto"
}
```

### 1.5 Deletar Projeto
```http
DELETE http://127.0.0.1:8000/api/projects/{project_id}/
```

---

## 2️⃣ UPLOAD DE ARQUIVOS

### 2.1 Upload Arquivo CSV (sem target column)
```bash
# Via cURL
curl -X POST http://127.0.0.1:8000/api/projects/{project_id}/upload/ \
  -F "file=@dados.csv" \
  -F "delimiter=," \
  -F "target_label=classe_a" \
  -F "has_target_column=false"
```

### 2.2 Upload Arquivo TXT (com separador ponto-e-vírgula)
```bash
# Via cURL
curl -X POST http://127.0.0.1:8000/api/projects/{project_id}/upload/ \
  -F "file=@motor_ligado.txt" \
  -F "delimiter=;" \
  -F "target_label=ligado" \
  -F "has_target_column=false"
```

### 2.3 Upload Arquivo CSV (com target column já existente)
```bash
# Via cURL
curl -X POST http://127.0.0.1:8000/api/projects/{project_id}/upload/ \
  -F "file=@dataset_completo.csv" \
  -F "delimiter=," \
  -F "has_target_column=true"
```

### 2.4 Upload via Python
```python
import requests

url = "http://127.0.0.1:8000/api/projects/{project_id}/upload/"

files = {
    'file': open('motor_ligado.txt', 'rb')
}

data = {
    'delimiter': ';',
    'target_label': 'ligado',
    'has_target_column': 'false'
}

response = requests.post(url, files=files, data=data)
print(response.json())
```

---

## 3️⃣ PREVIEW DE DADOS

### 3.1 Visualizar Dados Consolidados
```http
GET http://127.0.0.1:8000/api/projects/{project_id}/preview/
```

**Resposta esperada:**
```json
{
    "project": {
        "id": "uuid",
        "name": "Motor Classifier",
        "status": "ready",
        "data_files_count": 3,
        "models_count": 0
    },
    "statistics": {
        "total_rows": 1500,
        "total_columns": 5,
        "columns": ["x", "y", "z", "adc_raw", "target"],
        "missing_values": {
            "x": 0,
            "y": 0,
            "z": 0,
            "adc_raw": 0,
            "target": 0
        },
        "column_stats": {
            "x": {
                "type": "integer",
                "mean": 1234.5,
                "std": 456.7,
                "min": -2000,
                "max": 2000
            }
        },
        "outliers_count": 15,
        "outliers_percentage": 1.0
    },
    "preview_rows": [
        {
            "x": 1234,
            "y": -567,
            "z": 890,
            "adc_raw": 2048,
            "target": "ligado"
        }
    ],
    "total_files": 3
}
```

---

## 4️⃣ TREINAMENTO DE MODELOS

### 4.1 Treinar Modelo
```http
POST http://127.0.0.1:8000/api/models/train/
Content-Type: application/json

{
    "project_id": "abc-123-def-456",
    "model_name": "Motor Classifier v1",
    "version": "1.0.0",
    "feature_columns": ["x", "y", "z", "adc_raw"],
    "target_column": "target",
    "epochs": 100,
    "batch_size": 32,
    "test_size": 0.2
}
```

### 4.2 Treinar Modelo (configuração rápida para testes)
```json
{
    "project_id": "abc-123-def-456",
    "model_name": "Test Model",
    "feature_columns": ["x", "y", "z", "adc_raw"],
    "target_column": "target",
    "epochs": 10,
    "batch_size": 16
}
```

### 4.3 Via Python
```python
import requests

url = "http://127.0.0.1:8000/api/models/train/"

payload = {
    "project_id": "abc-123-def-456",
    "model_name": "Motor Classifier v1",
    "feature_columns": ["x", "y", "z", "adc_raw"],
    "target_column": "target",
    "epochs": 100,
    "batch_size": 32,
    "test_size": 0.2
}

response = requests.post(url, json=payload)
print(response.json())
```

---

## 5️⃣ MODELOS

### 5.1 Listar Todos os Modelos
```http
GET http://127.0.0.1:8000/api/models/
```

### 5.2 Filtrar Modelos por Projeto
```http
GET http://127.0.0.1:8000/api/models/?project={project_id}
```

### 5.3 Obter Detalhes de um Modelo
```http
GET http://127.0.0.1:8000/api/models/{model_id}/
```

**Resposta esperada:**
```json
{
    "id": "model-uuid",
    "project": "project-uuid",
    "project_name": "Motor Classifier",
    "name": "Motor Classifier v1",
    "version": "1.0.0",
    "status": "completed",
    "feature_columns": ["x", "y", "z", "adc_raw"],
    "target_column": "target",
    "epochs": 100,
    "batch_size": 32,
    "test_size": 0.2,
    "keras_model_url": "http://127.0.0.1:8000/media/models/.../model.h5",
    "tflite_model_url": "http://127.0.0.1:8000/media/models/.../model.tflite",
    "scaler_url": "http://127.0.0.1:8000/media/models/.../scaler.pkl",
    "accuracy": 0.9567,
    "loss": 0.1234,
    "model_size_kb": 45.67,
    "training_time_seconds": 123.45,
    "created_at": "2025-10-07T14:00:00Z",
    "trained_at": "2025-10-07T14:05:00Z"
}
```

### 5.4 Download do Modelo TFLite
```http
GET http://127.0.0.1:8000/api/models/{model_id}/download/
```

**Resposta:**
```json
{
    "download_url": "http://127.0.0.1:8000/media/models/uuid/Model.tflite",
    "file_name": "Model.tflite",
    "size_kb": 45.67
}
```

### 5.5 Obter Métricas Detalhadas
```http
GET http://127.0.0.1:8000/api/models/{model_id}/metrics/
```

**Resposta:**
```json
{
    "model_id": "uuid",
    "model_name": "Motor Classifier v1",
    "version": "1.0.0",
    "accuracy": 0.9567,
    "loss": 0.1234,
    "training_history": {
        "loss": [0.5, 0.4, 0.3, 0.2, 0.15, 0.12],
        "accuracy": [0.7, 0.8, 0.85, 0.9, 0.93, 0.96],
        "val_loss": [0.6, 0.45, 0.35, 0.25, 0.18, 0.15],
        "val_accuracy": [0.65, 0.75, 0.82, 0.88, 0.91, 0.94]
    },
    "confusion_matrix": [
        [95, 3, 2],
        [2, 98, 0],
        [1, 0, 99]
    ],
    "classification_report": {
        "ligado": {
            "precision": 0.97,
            "recall": 0.95,
            "f1-score": 0.96,
            "support": 100
        },
        "desligado": {
            "precision": 0.97,
            "recall": 0.98,
            "f1-score": 0.975,
            "support": 100
        },
        "defeito": {
            "precision": 0.98,
            "recall": 0.99,
            "f1-score": 0.985,
            "support": 100
        }
    },
    "training_config": {
        "feature_columns": ["x", "y", "z", "adc_raw"],
        "target_column": "target",
        "epochs": 100,
        "batch_size": 32,
        "test_size": 0.2
    },
    "training_time_seconds": 123.45,
    "model_size_kb": 45.67,
    "trained_at": "2025-10-07T14:05:00Z"
}
```

### 5.6 Deletar Modelo
```http
DELETE http://127.0.0.1:8000/api/models/{model_id}/
```

---

## 6️⃣ ARQUIVOS DE DADOS

### 6.1 Listar Arquivos de um Projeto
```http
GET http://127.0.0.1:8000/api/datafiles/?project={project_id}
```

### 6.2 Obter Detalhes de um Arquivo
```http
GET http://127.0.0.1:8000/api/datafiles/{datafile_id}/
```

### 6.3 Deletar Arquivo
```http
DELETE http://127.0.0.1:8000/api/datafiles/{datafile_id}/
```

---

## 🔄 FLUXO COMPLETO - Exemplo Prático

### Exemplo: Classificador de Motor (3 classes)

```python
import requests
import time

BASE_URL = "http://127.0.0.1:8000/api"

# 1. Criar Projeto
print("1️⃣ Criando projeto...")
response = requests.post(f"{BASE_URL}/projects/", json={
    "name": "Motor Classifier - Teste Completo",
    "description": "Detectar: ligado, desligado, defeito"
})
project = response.json()
project_id = project['id']
print(f"✅ Projeto criado: {project_id}")

# 2. Upload de 3 arquivos
arquivos = [
    ("motor_ligado.txt", "ligado"),
    ("motor_desligado.txt", "desligado"),
    ("motor_defeito.txt", "defeito")
]

print("\n2️⃣ Fazendo upload dos arquivos...")
for arquivo, label in arquivos:
    with open(arquivo, 'rb') as f:
        files = {'file': f}
        data = {
            'delimiter': ';',
            'target_label': label,
            'has_target_column': 'false'
        }
        response = requests.post(
            f"{BASE_URL}/projects/{project_id}/upload/",
            files=files,
            data=data
        )
        print(f"✅ Upload: {arquivo} -> {response.status_code}")

# 3. Preview dos dados
print("\n3️⃣ Visualizando preview dos dados...")
response = requests.get(f"{BASE_URL}/projects/{project_id}/preview/")
preview = response.json()
print(f"✅ Total de linhas: {preview['total_rows']}")
print(f"✅ Colunas: {preview['columns']}")

# 4. Treinar modelo
print("\n4️⃣ Iniciando treinamento do modelo...")
response = requests.post(f"{BASE_URL}/models/train/", json={
    "project_id": project_id,
    "model_name": "Motor Classifier v1.0",
    "feature_columns": ["x", "y", "z", "adc_raw"],
    "target_column": "target",
    "epochs": 50,
    "batch_size": 32,
    "test_size": 0.2
})
model = response.json()
model_id = model['id']
print(f"✅ Modelo criado: {model_id}")
print(f"⏳ Status: {model['status']}")

# 5. Aguardar treinamento (verificar status)
print("\n5️⃣ Aguardando conclusão do treinamento...")
while True:
    response = requests.get(f"{BASE_URL}/models/{model_id}/")
    model = response.json()
    status = model['status']
    print(f"Status: {status}")
    
    if status == 'completed':
        print(f"✅ Treinamento concluído!")
        print(f"📊 Acurácia: {model['accuracy']:.4f}")
        print(f"📊 Loss: {model['loss']:.4f}")
        break
    elif status == 'failed':
        print(f"❌ Treinamento falhou: {model.get('error_message')}")
        break
    
    time.sleep(5)  # Aguardar 5 segundos

# 6. Obter métricas
print("\n6️⃣ Obtendo métricas detalhadas...")
response = requests.get(f"{BASE_URL}/models/{model_id}/metrics/")
metrics = response.json()
print(f"✅ Confusion Matrix:")
print(metrics['confusion_matrix'])

# 7. Download do modelo
print("\n7️⃣ Fazendo download do modelo TFLite...")
response = requests.get(f"{BASE_URL}/models/{model_id}/download/")
download_info = response.json()
print(f"✅ Download URL: {download_info['download_url']}")
print(f"✅ Tamanho: {download_info['size_kb']:.2f} KB")

# Baixar o arquivo
tflite_response = requests.get(download_info['download_url'])
with open('motor_classifier.tflite', 'wb') as f:
    f.write(tflite_response.content)
print(f"✅ Modelo salvo como: motor_classifier.tflite")

print("\n🎉 Processo completo finalizado!")
```

---

## 🧪 Testes Rápidos

### Teste 1: Criar e Listar Projetos
```bash
# Criar
curl -X POST http://127.0.0.1:8000/api/projects/ \
  -H "Content-Type: application/json" \
  -d '{"name":"Teste 1","description":"Projeto de teste"}'

# Listar
curl http://127.0.0.1:8000/api/projects/
```

### Teste 2: Upload de Arquivo CSV Simples
```bash
# Criar arquivo de teste
echo "col1,col2,col3,label
1,2,3,A
4,5,6,B
7,8,9,A" > teste.csv

# Upload
curl -X POST http://127.0.0.1:8000/api/projects/{project_id}/upload/ \
  -F "file=@teste.csv" \
  -F "delimiter=," \
  -F "has_target_column=true"
```

### Teste 3: Treino Rápido (10 épocas)
```bash
curl -X POST http://127.0.0.1:8000/api/models/train/ \
  -H "Content-Type: application/json" \
  -d '{
    "project_id": "seu-project-id",
    "model_name": "Teste Rápido",
    "feature_columns": ["col1", "col2", "col3"],
    "target_column": "label",
    "epochs": 10,
    "batch_size": 16
  }'
```

---

## 📝 Notas

- Substitua `{project_id}`, `{model_id}`, `{datafile_id}` pelos IDs reais retornados pela API
- O treinamento é síncrono, então a requisição pode demorar dependendo do dataset e configurações
- Modelos TFLite são automaticamente quantizados para int8
- Todos os endpoints retornam JSON

---

## 🔧 Ferramentas Recomendadas

- **Postman**: https://www.postman.com/
- **Insomnia**: https://insomnia.rest/
- **HTTPie**: https://httpie.io/
- **Python requests**: `pip install requests`

**Dica**: Use o Django Admin (http://127.0.0.1:8000/admin/) para visualizar todos os dados no banco!

