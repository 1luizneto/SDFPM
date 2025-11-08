"""
Script para verificar modelos disponíveis na API
"""

import requests
import json
from datetime import datetime

BASE_URL = "http://127.0.0.1:8000"


def list_all_models():
    """Lista todos os modelos disponíveis"""
    print("🔍 Buscando modelos disponíveis...")
    print("=" * 60)

    try:
        response = requests.get(f"{BASE_URL}/api/models/")

        if response.status_code == 200:
            data = response.json()

            # Verificar se há resultados
            if 'results' in data:
                models = data['results']
            else:
                models = data

            if not models:
                print("❌ Nenhum modelo encontrado")
                return None

            print(f"✅ {len(models)} modelo(s) encontrado(s):\n")

            selected_model = None

            for i, model in enumerate(models, 1):
                print(f"Modelo {i}:")
                print(f"  ID: {model['id']}")
                print(f"  Nome: {model['name']}")
                print(f"  Projeto: {model.get('project_name', model.get('project', 'N/A'))}")
                print(f"  Status: {model['status']}")
                print(f"  Acurácia: {model.get('accuracy', 'N/A')}")

                # Verificar se tem arquivo TFLite
                if model['status'] == 'completed':
                    print(f"  ✅ Modelo treinado e disponível")
                    if not selected_model:
                        selected_model = model['id']
                else:
                    print(f"  ⚠️ Status: {model['status']}")

                print("-" * 40)

            return selected_model

        else:
            print(f"❌ Erro ao buscar modelos: Status {response.status_code}")
            print(f"Resposta: {response.text}")
            return None

    except Exception as e:
        print(f"❌ Erro ao conectar com a API: {e}")
        return None


def check_model_files(model_id):
    """Verifica se os arquivos do modelo existem"""
    print(f"\n🔍 Verificando arquivos do modelo {model_id}...")
    print("-" * 60)

    files_to_check = {
        'Modelo Keras': f"{BASE_URL}/api/models/{model_id}/",
        'Download TFLite': f"{BASE_URL}/api/models/{model_id}/download/",
        'Métricas': f"{BASE_URL}/api/models/{model_id}/metrics/",
    }

    model_details = None

    for name, url in files_to_check.items():
        try:
            response = requests.get(url)
            print(f"{name}: ", end="")

            if response.status_code == 200:
                print("✅ Disponível")
                if name == 'Modelo Keras':
                    model_details = response.json()
            else:
                print(f"❌ Status {response.status_code}")

        except Exception as e:
            print(f"❌ Erro: {e}")

    if model_details:
        print(f"\n📊 Detalhes do Modelo:")
        print(f"  Nome: {model_details.get('name', 'N/A')}")
        print(f"  Versão: {model_details.get('version', 'N/A')}")
        print(f"  Features: {model_details.get('feature_columns', [])}")
        print(f"  Target: {model_details.get('target_column', 'N/A')}")
        print(f"  Acurácia: {model_details.get('accuracy', 'N/A')}")

        # Verificar URLs dos arquivos
        print(f"\n📁 URLs dos Arquivos:")
        if model_details.get('keras_model_url'):
            print(f"  Keras: {model_details['keras_model_url']}")
        if model_details.get('tflite_model_url'):
            print(f"  TFLite: {model_details['tflite_model_url']}")
        if model_details.get('scaler_url'):
            print(f"  Scaler: {model_details['scaler_url']}")

        # Testar se os arquivos realmente existem
        print(f"\n🧪 Testando acesso direto aos arquivos:")

        # Extrair nome do arquivo do modelo
        if model_details.get('keras_model_url'):
            # Extrair o nome do arquivo da URL
            keras_url = model_details['keras_model_url']
            # Tentar acessar
            try:
                response = requests.head(keras_url)
                if response.status_code == 200:
                    print(f"  ✅ Arquivo Keras acessível")
                else:
                    print(f"  ❌ Arquivo Keras não encontrado (Status {response.status_code})")
            except:
                print(f"  ❌ Erro ao acessar arquivo Keras")

        return model_details

    return None


def suggest_test_script_update(model_id):
    """Sugere como atualizar o script de teste"""
    print(f"\n💡 PARA USAR ESTE MODELO NO SCRIPT DE TESTE:")
    print("=" * 60)
    print(f"Atualize a linha no arquivo test_model.py:")
    print(f'\nMODEL_ID = "{model_id}"')
    print(f"\nOu crie uma cópia do script com o ID correto.")


def main():
    print("🚀 VERIFICADOR DE MODELOS NEXUSML")
    print("=" * 60)

    # Listar todos os modelos
    selected_model = list_all_models()

    if selected_model:
        print(f"\n📌 Modelo selecionado para teste: {selected_model}")

        # Verificar arquivos do modelo
        model_details = check_model_files(selected_model)

        if model_details:
            suggest_test_script_update(selected_model)

            # Perguntar se quer testar outro modelo
            print("\n" + "=" * 60)
            outro = input("Deseja verificar outro modelo? Digite o ID ou 'n' para sair: ")
            if outro.lower() != 'n' and outro:
                check_model_files(outro)
                suggest_test_script_update(outro)
    else:
        print("\n⚠️ Nenhum modelo disponível para teste")
        print("Certifique-se de que:")
        print("1. O servidor Django está rodando")
        print("2. Você tem modelos treinados com status 'completed'")


if __name__ == "__main__":
    main()