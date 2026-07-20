#!/usr/bin/env python3
"""
NexusControl - Gateway Serial para IP (Edição Monitor de Postura)
Este script lê dados da porta Serial (USB) enviados pelo Arduino do Wearable,
e envia as leituras de distância em tempo real para a API do NexusControl.
"""

import sys
import time
import json
import requests

# CONFIGURAÇÕES DA API NEXUSCONTROL
API_URL = "http://localhost:8000"
SENSOR_ID = 1  # ID do sensor de postura cadastrado no populate_data.py
READING_ENDPOINT = f"{API_URL}/api/v1/sensors/{SENSOR_ID}/reading/"

# CONFIGURAÇÕES SERIAL
# No Linux: Geralmente /dev/ttyACM0 ou /dev/ttyUSB0. No Windows: COM3, COM4, etc.
SERIAL_PORT = "/dev/ttyACM0" 
BAUD_RATE = 9600

def enviar_leitura(valor):
    """Envia o valor de distância lido pelo sensor para o Django."""
    payload = {"value": valor}
    try:
        # O endpoint /reading/ foi liberado para acesso sem autenticação (AllowAny)
        # permitindo o envio rápido e direto de dispositivos IoT sem overhead de JWT.
        response = requests.post(READING_ENDPOINT, json=payload, timeout=3)
        if response.status_code == 200:
            status_api = response.json().get('sensor_status', 'desconhecido')
            print(f"🚀 [Enviado] Distância: {valor} cm | Status Corporal: {status_api.upper()}")
        else:
            print(f"❌ Falha no envio. Status HTTP: {response.status_code}, Detalhes: {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"❌ Erro de conexão com o servidor NexusControl: {e}")

def main():
    try:
        import serial
    except ImportError:
        print("❌ Biblioteca 'pyserial' não encontrada. Instale executando: pip install pyserial")
        sys.exit(1)

    print("====================================================")
    print("      NEXUSCONTROL - GATEWAY POSTURA WEARABLE       ")
    print("====================================================")
    print(f"Porta Serial: {SERIAL_PORT} | Baudrate: {BAUD_RATE}")
    print(f"Endpoint API: {READING_ENDPOINT}")
    print("----------------------------------------------------")

    ser = None
    tentativas = 0
    while not ser:
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
            ser.flush()
            print("🔌 Conexão estabelecida com o Arduino via USB Serial!")
        except Exception as e:
            tentativas += 1
            print(f"⚠️ Aguardando conexão na porta serial {SERIAL_PORT}... (Tentativa {tentativas}) - Erro: {e}")
            time.sleep(3)
            if tentativas >= 5:
                print("\nDica: Verifique se o seu Arduino está conectado na USB correta.")
                print("No Linux, você pode listar as portas com: ls /dev/tty*")
                print("E dar permissão com: sudo chmod 666 /dev/ttyACM0\n")
                tentativas = 0

    # Loop de leitura e retransmissão
    try:
        while True:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue
                
                # Ignora mensagens informativas brutas
                if not line.startswith("{"):
                    print(f"💬 [Arduino Serial]: {line}")
                    continue
                
                try:
                    data = json.loads(line)
                    if "distance" in data:
                        distancia = float(data["distance"])
                        enviar_leitura(distancia)
                    elif "status" in data:
                        print(f"⚙️ [Arduino Status]: {data['status'].upper()}")
                        if "base_distance" in data:
                            print(f"📏 Distância base calibrada pelo usuário: {data['base_distance']} cm")
                    elif "error" in data:
                        print(f"⚠️ Erro reportado pelo Arduino: {data['error']}")
                except json.JSONDecodeError:
                    # Mensagem malformada (ruído de linha), ignora
                    pass
            time.sleep(0.05)
            
    except KeyboardInterrupt:
        print("\n👋 Gateway de Postura encerrado pelo usuário.")
        if ser:
            ser.close()
            print("🔌 Porta Serial fechada.")

if __name__ == "__main__":
    main()
