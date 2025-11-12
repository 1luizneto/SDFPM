# 🚀 Guia Rápido - NexusML API com Swagger

## 📍 URLs Principais

Depois de iniciar o servidor com `python manage.py runserver`:

### 🏠 Página Inicial
```
http://127.0.0.1:8000/
```
**Página de boas-vindas** com links para todas as funcionalidades!

---

### 📚 Documentação Swagger (RECOMENDADO)
```
http://127.0.0.1:8000/api/docs/
```
**Interface interativa** onde você pode:
- ✅ Ver todos os endpoints
- ✅ Testar requisições diretamente no navegador
- ✅ Ver exemplos de request/response
- ✅ Fazer upload de arquivos
- ✅ Visualizar schemas

---

### 📖 ReDoc (Alternativa)
```
http://127.0.0.1:8000/api/redoc/
```
Documentação em formato de livro, ideal para leitura.

---

### ⚙️ Django Admin
```
http://127.0.0.1:8000/admin/
```
Painel administrativo para ver dados no banco.

---

### 🔌 API Root
```
http://127.0.0.1:8000/api/
```
Interface navegável do DRF com lista de endpoints.

---

## 🧪 Como Usar o Swagger UI

### 1. Acesse a documentação
Abra no navegador: `http://127.0.0.1:8000/api/docs/`

### 2. Explore os endpoints
Você verá 3 categorias principais:
- **Projects** - Gerenciamento de projetos
- **Data Files** - Arquivos de dados
- **Models** - Modelos de ML

### 3. Teste um endpoint
Por exemplo, para criar um projeto:

1. Clique em **POST /api/projects/**
2. Clique no botão **"Try it out"**
3. Edite o JSON de exemplo:
   ```json
   {
     "name": "Meu Primeiro Projeto",
     "description": "Teste do Swagger"
   }
   ```
4. Clique em **"Execute"**
5. Veja a resposta abaixo!

### 4. Upload de arquivo
Para testar upload:

1. Clique em **POST /api/projects/{id}/upload/**
2. Clique em **"Try it out"**
3. Insira o ID do projeto
4. Clique em **"Choose File"** para selecionar arquivo
5. Preencha os parâmetros:
   - `delimiter`: `,` ou `;`
   - `target_label`: `classe_a`
6. Clique em **"Execute"**

### 5. Treinar modelo
1. Clique em **POST /api/models/train/**
2. Clique em **"Try it out"**
3. Use o exemplo fornecido ou edite:
   ```json
   {
     "project_id": "seu-uuid-aqui",
     "model_name": "Meu Modelo",
     "feature_columns": ["col1", "col2", "col3"],
     "target_column": "target",
     "epochs": 10,
     "batch_size": 16
   }
   ```
4. Clique em **"Execute"**
5. Aguarde o treinamento (pode demorar alguns minutos)

---

## 🎯 Fluxo Rápido de Teste

### Via Swagger UI:

1. **Criar Projeto**
   - POST `/api/projects/`
   - Body: `{"name": "Teste", "description": "..."}`
   - Copie o `id` da resposta

2. **Upload Arquivo**
   - POST `/api/projects/{id}/upload/`
   - Escolha um arquivo CSV/TXT
   - Configure delimiter e target_label

3. **Ver Preview**
   - GET `/api/projects/{id}/preview/`
   - Visualize os dados processados

4. **Treinar Modelo**
   - POST `/api/models/train/`
   - Configure feature_columns e target_column
   - Aguarde o treinamento

5. **Download do Modelo**
   - GET `/api/models/{model_id}/download/`
   - Copie a `download_url`
   - Abra no navegador para baixar o .tflite

---

## 🔑 Autenticação

Por enquanto, a API está **sem autenticação** (modo desenvolvimento).

Para acessar o Django Admin:
- URL: `http://127.0.0.1:8000/admin/`
- Usuário: `admin`
- Senha: [a que você definiu]

---

## 💡 Dicas

### No Swagger UI:
- ✅ Clique nas setas `▼` para expandir endpoints
- ✅ Use "Try it out" para testar
- ✅ Veja "Schemas" no final da página para estruturas de dados
- ✅ Clique em "Download" no topo para baixar o schema OpenAPI

### Atalhos de Teclado:
- `Ctrl + K` - Abrir busca
- `Ctrl + /` - Focar na busca

### Exemplos Prontos:
Cada endpoint tem exemplos de request que você pode usar diretamente!

---

## 🐛 Problemas Comuns

### "404 Not Found" ao acessar /docs
❌ URL errada: `http://127.0.0.1:8000/docs`
✅ URL correta: `http://127.0.0.1:8000/api/docs/`

### Upload de arquivo não funciona
- Verifique se o arquivo é CSV ou TXT
- Certifique-se que o delimiter está correto
- Tamanho máximo: 100 MB

### Erro no treinamento
- Verifique se o projeto tem arquivos
- Confirme que as colunas especificadas existem
- Use epochs baixo (10-20) para testes rápidos

---

## 📱 Testando Externamente

Se quiser testar de outro dispositivo na mesma rede:

1. Descubra seu IP: `ipconfig` (Windows) ou `ifconfig` (Mac/Linux)
2. Adicione o IP em `settings.py`:
   ```python
   ALLOWED_HOSTS = ['127.0.0.1', 'localhost', 'SEU.IP.AQUI']
   ```
3. Acesse de outro dispositivo: `http://SEU.IP:8000/api/docs/`

---

## 🎨 Personalizando o Swagger

As configurações estão em `settings.py`:
```python
SPECTACULAR_SETTINGS = {
    'TITLE': 'NexusML API',
    'DESCRIPTION': '...',
    'VERSION': '1.0.0',
    # ... mais configurações
}
```

---

**Pronto para começar! Acesse http://127.0.0.1:8000/ e explore a API! 🚀**

