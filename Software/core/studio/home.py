from django.http import HttpResponse
from django.shortcuts import render

def home_view(request):
    """View para página inicial com links úteis"""
    html = """
    <!DOCTYPE html>
    <html lang="pt-BR">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>NexusML API</title>
        <style>
            * {
                margin: 0;
                padding: 0;
                box-sizing: border-box;
            }
            body {
                font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
                background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                min-height: 100vh;
                display: flex;
                align-items: center;
                justify-content: center;
                padding: 20px;
            }
            .container {
                background: white;
                border-radius: 20px;
                padding: 40px;
                max-width: 800px;
                box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            }
            h1 {
                color: #667eea;
                margin-bottom: 10px;
                font-size: 2.5em;
            }
            .subtitle {
                color: #666;
                margin-bottom: 30px;
                font-size: 1.1em;
            }
            .links {
                display: grid;
                grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
                gap: 15px;
                margin-top: 30px;
            }
            .link-card {
                background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                color: white;
                padding: 20px;
                border-radius: 12px;
                text-decoration: none;
                transition: transform 0.3s, box-shadow 0.3s;
                display: block;
            }
            .link-card:hover {
                transform: translateY(-5px);
                box-shadow: 0 10px 25px rgba(102, 126, 234, 0.4);
            }
            .link-card h3 {
                margin-bottom: 8px;
                font-size: 1.2em;
            }
            .link-card p {
                font-size: 0.9em;
                opacity: 0.9;
            }
            .status {
                background: #d4edda;
                color: #155724;
                padding: 12px;
                border-radius: 8px;
                margin-bottom: 20px;
                border-left: 4px solid #28a745;
            }
            .feature-list {
                background: #f8f9fa;
                padding: 20px;
                border-radius: 10px;
                margin-top: 20px;
            }
            .feature-list h3 {
                color: #667eea;
                margin-bottom: 15px;
            }
            .feature-list ul {
                list-style: none;
                padding: 0;
            }
            .feature-list li {
                padding: 8px 0;
                padding-left: 25px;
                position: relative;
            }
            .feature-list li:before {
                content: "✓";
                position: absolute;
                left: 0;
                color: #28a745;
                font-weight: bold;
            }
            .version {
                text-align: center;
                color: #999;
                margin-top: 30px;
                font-size: 0.9em;
            }
        </style>
    </head>
    <body>
        <div class="container">
            <h1>🚀 NexusML API</h1>
            <p class="subtitle">API REST para geração de modelos TinyML</p>
            
            <div class="status">
                <strong>✅ Servidor Online</strong> - Todos os sistemas funcionando corretamente!
            </div>
            
            <div class="links">
                <a href="/api/docs/" class="link-card">
                    <h3>📚 Swagger UI</h3>
                    <p>Interface interativa para testar os endpoints da API</p>
                </a>
                
                <a href="/api/redoc/" class="link-card">
                    <h3>📖 ReDoc</h3>
                    <p>Documentação em formato de livro</p>
                </a>
                
                <a href="/admin/" class="link-card">
                    <h3>⚙️ Django Admin</h3>
                    <p>Painel administrativo do sistema</p>
                </a>
                
                <a href="/api/" class="link-card">
                    <h3>🔌 API Root</h3>
                    <p>Navegação pelos endpoints</p>
                </a>
                
                <a href="/api/projects/" class="link-card">
                    <h3>📁 Projetos</h3>
                    <p>Gerenciar projetos e datasets</p>
                </a>
                
                <a href="/api/models/" class="link-card">
                    <h3>🤖 Modelos</h3>
                    <p>Ver modelos treinados</p>
                </a>
            </div>
            
            <div class="feature-list">
                <h3>🎯 Funcionalidades</h3>
                <ul>
                    <li>Upload de arquivos CSV e TXT</li>
                    <li>Processamento automático de dados</li>
                    <li>Treinamento de modelos CNN 1D</li>
                    <li>Conversão para TensorFlow Lite (quantizado)</li>
                    <li>Download de modelos .tflite</li>
                    <li>Métricas e estatísticas detalhadas</li>
                </ul>
            </div>
            
            <div class="version">
                <p>NexusML API v1.0.0 | Django 5.2.7 | DRF 3.16.1</p>
            </div>
        </div>
    </body>
    </html>
    """
    return HttpResponse(html)

