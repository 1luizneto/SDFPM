# SDFPM - Sistemas de Detecção de Falhas Preditiva de Motor DC


# 1. Objetivo Geral
Este projeto consiste no desenvolvimento de um sistema embarcado autônomo e inteligente para o monitoramento e diagnóstico de falhas em motores elétricos em tempo real. O objetivo é criar uma ferramenta de manutenção preditiva completa, desde a coleta de dados na ponta até a visualização e interação pelo usuário. O resultado será um protótipo funcional capaz de classificar a operação do motor como "normal", "desligado" ou "com falha/anomalia".

# 2. Arquitetura do Sistema
O sistema é composto por três pilares principais: o hardware embarcado, o aplicativo móvel e a plataforma de visualização de dados.

 ## Hardware Embarcado
* Microcontrolador: ESP32-S3, escolhido por seu poder de processamento dual-core, conectividade Wi-Fi/Bluetooth e capacidade para executar modelos de Machine Learning.

Sensores:
  * Unidade de Medição Inercial (IMU): MPU6500 para coletar dados de vibração de alta frequência nos três eixos (X, Y, Z).
  * Sensor de Corrente: Módulo para monitorar o consumo elétrico do motor, um indicador chave de seu esforço e eficiência.

# Software e Plataformas

* Firmware: Desenvolvido em C/C++ utilizando o framework ESP-IDF. Responsável pela coleta de dados, processamento e execução do modelo TinyML.

* Aplicativo Móvel (Android/iOS): Interface de usuário para configuração, controle e visualização rápida do status do motor.

* Dashboard de Monitoramento: Uma plataforma web, como o Grafana, para análise detalhada e histórica dos dados.

# 3. Funcionalidades Principais
 ## Coleta de Dados Multissensorial
O firmware do ESP32 irá coletar, em tempo real e de forma sincronizada, os dados de vibração do MPU6500 e o consumo de corrente do sensor dedicado.

# Processamento e Análise Embarcada com TinyML
Um modelo de Machine Learning, otimizado para ambientes com recursos limitados (TinyML), será treinado e implementado diretamente no ESP32-S3. Este modelo analisará os dados brutos para classificar o estado do motor, permitindo um diagnóstico instantâneo sem depender de conexão constante com a nuvem.

# Aplicativo Móvel para Interação e Controle
* Configuração do Dispositivo: Comunicação via Bluetooth Low Energy (BLE) para configuração inicial (credenciais de Wi-Fi, sensibilidade dos sensores, etc.).

* Visualização da "Saúde" do Motor: Exibição clara e intuitiva do status atual do motor (e.g., indicador verde/amarelo/vermelho).

* Modos de Operação: Permitir que o usuário alterne entre modos como "Monitoramento Contínuo" ou "Diagnóstico sob Demanda".

* Alertas e Notificações: Envio de notificações push ao usuário em caso de detecção de falha.

# Dashboard de Monitoramento Remoto (Grafana)
* Conectividade: O ESP32 enviará os dados coletados (vibração, corrente e diagnóstico) via Wi-Fi para um banco de dados.

* Visualização Histórica: Gráficos detalhados para exibir o histórico de operação do motor.

* Análise de Tendências: Ferramenta para que equipes de manutenção possam analisar tendências de longo prazo e identificar a degradação sutil do motor ao longo do tempo.
