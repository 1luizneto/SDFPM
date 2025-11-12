# NexusML API - API de Geração de Modelos TinyML

API REST desenvolvida com Django para geração automática de modelos de Machine Learning em formato TensorFlow Lite (TinyML) para sistemas embarcados.

## 🚀 Funcionalidades

- ✅ Upload de arquivos CSV e TXT
- ✅ Processamento automático de dados com delimitadores configuráveis
- ✅ Suporte a múltiplos arquivos por projeto
- ✅ Criação automática de variáveis alvo (target)
- ✅ Treinamento de modelos CNN 1D genéricos
- ✅ Conversão automática para TensorFlow Lite (quantizado)
- ✅ Download de modelos .tflite
- ✅ Visualização de métricas e estatísticas

## 📋 Pré-requisitos

- Python 3.10+
- Todas as dependências do `requirements.txt`

## 🔧 Instalação

```bash
# Clonar repositório
cd "NexusML API"

# Criar ambiente virtual
python -m venv venv

# Ativar ambiente virtual (Windows)
venv\Scripts\activate

# Instalar dependências
pip install -r requirements.txt

# Aplicar migrations
cd core
python manage.py migrate

# Criar superusuário
python manage.py createsuperuser

# Iniciar servidor
python manage.py runserver
```

## 📡 Endpoints da API

### Base URL
```
http://127.0.0.1:8000/api/
```

### 📚 Documentação Interativa (Swagger/OpenAPI)

Acesse a documentação interativa da API através do Swagger UI:

#### Swagger UI (Recomendado)
```
http://127.0.0.1:8000/api/docs/
```
Interface visual completa para testar todos os endpoints diretamente no navegador.

#### ReDoc (Alternativa)
```
http://127.0.0.1:8000/api/redoc/
```
Documentação em estilo de livro, ideal para leitura.

#### OpenAPI Schema (JSON)
```
http://127.0.0.1:8000/api/schema/
```
Schema OpenAPI 3.0 em formato JSON para integração com outras ferramentas.

---

### 1. **Projetos**

#### Listar todos os projetos
```http
GET /api/projects/
```

#### Criar novo projeto
```http
POST /api/projects/
Content-Type: application/json

{
    "name": "Motor Classifier",
    "description": "Classificação de estados do motor"
}
```

#### Obter detalhes de um projeto
```http
GET /api/projects/{project_id}/
```

#### Deletar projeto
```http
DELETE /api/projects/{project_id}/
```

---

### 2. **Upload de Arquivos**

#### Upload de arquivo para um projeto
```http
POST /api/projects/{project_id}/upload/
Content-Type: multipart/form-data

Form Data:
- file: [arquivo.csv ou arquivo.txt]
- delimiter: "," ou ";" ou outro
- target_label: "ligado" (opcional - label para este arquivo)
- has_target_column: "false" (se já tem coluna target, use "true")
```

**Exemplo com cURL:**
```bash
curl -X POST http://127.0.0.1:8000/api/projects/{project_id}/upload/ \
  -F "file=@motor_ligado.txt" \
  -F "delimiter=;" \
  -F "target_label=ligado" \
  -F "has_target_column=false"
```

---

### 3. **Preview de Dados**

#### Visualizar dados consolidados do projeto
```http
GET /api/projects/{project_id}/preview/
```

**Resposta:**
```json
{
    "project": {...},
    "statistics": {
        "total_rows": 1500,
        "total_columns": 5,
        "columns": ["x", "y", "z", "adc_raw", "target"],
        "column_stats": {...}
    },
    "preview_rows": [...],
    "total_files": 3
}
```

---

### 4. **Modelos de ML**

#### Listar todos os modelos
```http
GET /api/models/
```

#### Filtrar modelos por projeto
```http
GET /api/models/?project={project_id}
```

#### Treinar novo modelo
```http
POST /api/models/train/
Content-Type: application/json

{
    "project_id": "uuid-do-projeto",
    "model_name": "Motor Classifier v1",
    "version": "1.0.0",
    "feature_columns": ["x", "y", "z", "adc_raw"],
    "target_column": "target",
    "epochs": 100,
    "batch_size": 32,
    "test_size": 0.2
}
```

**Resposta:**
```json
{
    "id": "uuid-do-modelo",
    "name": "Motor Classifier v1",
    "status": "completed",
    "accuracy": 0.9567,
    "loss": 0.1234,
    "model_size_kb": 45.6,
    "training_time_seconds": 125.4,
    "tflite_model_url": "http://..."
}
```

#### Download do modelo TFLite
```http
GET /api/models/{model_id}/download/
```

**Resposta:**
```json
{
    "download_url": "http://127.0.0.1:8000/media/models/.../modelo.tflite",
    "file_name": "modelo.tflite",
    "size_kb": 45.6
}
```

#### Obter métricas detalhadas
```http
GET /api/models/{model_id}/metrics/
```

**Resposta:**
```json
{
    "model_id": "uuid",
    "accuracy": 0.9567,
    "loss": 0.1234,
    "confusion_matrix": [[100, 5], [3, 92]],
    "classification_report": {...},
    "training_history": {
        "loss": [...],
        "accuracy": [...],
        "val_loss": [...],
        "val_accuracy": [...]
    }
}
```

---

## 📊 Fluxo de Uso Completo

### Cenário 1: Múltiplos arquivos com criação de labels

```bash
# 1. Criar projeto
POST /api/projects/
{
    "name": "Motor Classifier",
    "description": "Detectar estados do motor"
}
# Resposta: { "id": "abc-123", ... }

# 2. Upload arquivo 1 - Motor Ligado
POST /api/projects/abc-123/upload/
FormData:
  - file: motor_ligado.txt
  - delimiter: ";"
  - target_label: "ligado"

# 3. Upload arquivo 2 - Motor Desligado
POST /api/projects/abc-123/upload/
FormData:
  - file: motor_desligado.txt
  - delimiter: ";"
  - target_label: "desligado"

# 4. Upload arquivo 3 - Motor com Defeito
POST /api/projects/abc-123/upload/
FormData:
  - file: motor_defeito.txt
  - delimiter: ";"
  - target_label: "defeito"

# 5. Verificar preview dos dados
GET /api/projects/abc-123/preview/

# 6. Treinar modelo
POST /api/models/train/
{
    "project_id": "abc-123",
    "model_name": "Motor Classifier",
    "feature_columns": ["x", "y", "z", "adc_raw"],
    "target_column": "target",
    "epochs": 100,
    "batch_size": 32
}

# 7. Aguardar treinamento e baixar modelo
GET /api/models/{model_id}/download/
```

### Cenário 2: Arquivo único com labels já incluídas

```bash
# 1. Criar projeto
POST /api/projects/
{
    "name": "Sensor Classifier"
}

# 2. Upload arquivo completo
POST /api/projects/{id}/upload/
FormData:
  - file: dataset_completo.csv
  - delimiter: ","
  - has_target_column: "true"

# 3. Treinar modelo
POST /api/models/train/
{
    "project_id": "{id}",
    "model_name": "Sensor Model",
    "feature_columns": ["sensor1", "sensor2", "sensor3"],
    "target_column": "label"
}
```

---

## 🏗️ Arquitetura

```
NexusML API/
├── context/
│   └── task-implementation-plan.md    # Plano de implementação
├── core/
│   ├── manage.py
│   ├── core/
│   │   ├── settings.py                # Configurações Django
│   │   └── urls.py                    # URLs principais
│   └── studio/
│       ├── models.py                  # Models: Project, DataFile, MLModel
│       ├── serializers.py             # Serializers DRF
│       ├── views.py                   # ViewSets e endpoints
│       ├── services.py                # GenericDataProcessor & GenericCNNTrainer
│       └── urls.py                    # URLs do app
└── requirements.txt
```

---

## 🔍 Modelos Django

### Project
- Agrupa datasets e modelos
- Status: creating, ready, processing, error

### DataFile
- Arquivo de dados (CSV/TXT)
- Configurações: delimiter, target_label, has_target_column
- Metadados: rows_count, columns (estatísticas)

### MLModel
- Modelo treinado
- Arquivos: keras_model (.h5), tflite_model (.tflite), scaler (.pkl)
- Métricas: accuracy, loss, confusion_matrix, classification_report
- Status: pending, training, completed, failed

---

## 📈 Processamento de Dados

O `GenericDataProcessor` realiza:
- ✅ Parse de CSV/TXT com delimitadores configuráveis
- ✅ Detecção automática de tipos de colunas
- ✅ Geração de estatísticas descritivas
- ✅ Detecção de outliers (zscore ou IQR)
- ✅ Consolidação de múltiplos arquivos
- ✅ Adição de labels automáticos

## 🤖 Treinamento de Modelos

O `GenericCNNTrainer` implementa:
- ✅ CNN 1D genérica e adaptável
- ✅ Normalização automática (StandardScaler)
- ✅ Encoding de labels categóricos
- ✅ Early stopping
- ✅ Conversão para TFLite com quantização int8
- ✅ Métricas completas (accuracy, loss, confusion matrix, classification report)

---

## 🔐 Admin Django

Acesse: `http://127.0.0.1:8000/admin/`

Usuário: `admin`
Senha: admin123

---

## 🧪 Testes

### Testar com Postman ou Insomnia

Importe a coleção de endpoints ou teste manualmente seguindo os exemplos acima.

### Testar com Python

```python
import requests

BASE_URL = "http://127.0.0.1:8000/api"

# Criar projeto
response = requests.post(f"{BASE_URL}/projects/", json={
    "name": "Teste Motor",
    "description": "Projeto de teste"
})
project_id = response.json()['id']

# Upload arquivo
with open('motor_ligado.txt', 'rb') as f:
    files = {'file': f}
    data = {
        'delimiter': ';',
        'target_label': 'ligado'
    }
    response = requests.post(
        f"{BASE_URL}/projects/{project_id}/upload/",
        files=files,
        data=data
    )

print(response.json())
```

---

## 📝 Notas Importantes

### Limites de Upload
- Tamanho máximo por arquivo: 100 MB
- Configurável em `settings.py`: `FILE_UPLOAD_MAX_MEMORY_SIZE`

### Formatos Suportados
- **CSV**: Delimitadores: `,` `;` `\t` `|`
- **TXT**: Parser genérico com detecção de estrutura

### Performance
- Treinamento é síncrono (implementação assíncrona com Celery está planejada)
- Para datasets grandes, considere reduzir epochs ou batch_size

---

## 🛠️ Troubleshooting

### Erro ao processar arquivo
- Verifique o delimitador correto
- Certifique-se que o arquivo não está vazio
- Verifique encoding (UTF-8)

### Erro no treinamento
- Verifique se as colunas especificadas existem no dataset
- Certifique-se que há dados suficientes (mínimo ~100 amostras)
- Verifique se a coluna target está correta

### Modelo muito grande
- Use quantização (já habilitada por padrão)
- Reduza número de camadas ou filtros (futuro: configurável)

---

## 🎯 Próximas Funcionalidades (Roadmap)

- [ ] Treinamento assíncrono com Celery
- [ ] WebSocket para updates em tempo real
- [ ] Configuração de arquitetura do modelo via API
- [ ] Suporte a mais algoritmos (Random Forest, XGBoost)
- [ ] Visualização de gráficos de treino
- [ ] Validação cruzada
- [ ] Grid search de hiperparâmetros
- [ ] API de inferência (predict)
- [ ] Autenticação JWT
- [ ] Rate limiting


