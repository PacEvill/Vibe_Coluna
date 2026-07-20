# 🔌 Integração Arduino/ESP & NexusControl

Este diretório contém os sketches de firmware para Arduino/ESP e scripts de gateway para conectar sensores físicos à central de comando IoT **NexusControl**.

---

## 🏗️ Hardware Suportado & Esquema de Ligação

Nossos esquemas e firmwares são baseados no sensor sônico **HC-SR04** (incluso em kits Master de Arduino) e um **Buzzer** para alerta local.

### Esquema de Conexão dos Componentes

| Sensor/Atuador | Pino no Componente | Pino no Arduino Uno | Pino no ESP8266 (NodeMCU) |
| :--- | :--- | :--- | :--- |
| **HC-SR04** | VCC | 5V | 3.3V / 5V |
| **HC-SR04** | GND | GND | GND |
| **HC-SR04** | Trig | D2 (Digital 2) | D6 (GPIO12) |
| **HC-SR04** | Echo | D3 (Digital 3) | D5 (GPIO14) |
| **Buzzer** | VCC (+) | D4 (Digital 4) | D2 (GPIO4) |
| **Buzzer** | GND (-) | GND | GND |

---

## 🚀 Como Iniciar a Integração (Passo a Passo)

### Passo 1: Preparar o Banco de Dados do NexusControl

Antes de conectar as placas, precisamos cadastrar o tipo de sensor e o sensor correspondente no painel do Django. Criamos um script automatizado para isso:

1. Ative seu ambiente virtual do Django:
   ```bash
   source .venv_new/bin/activate
   ```
2. Execute o script de configuração:
   ```bash
   python create_distance_sensor.py
   ```
3. O script criará o tipo de sensor **Distância** e o sensor **Ultrassônico Arduino HC-SR04** no banco. **Anote o ID gerado pelo terminal (geralmente `6` se você usou o banco padrão).**

---

### Opção A: Conexão Direta via Wi-Fi (ESP8266 ou ESP32)

Se o seu kit possui um módulo Wi-Fi independente (ou placa NodeMCU/ESP32):

1. Abra o arquivo [ultrasonic_sensor_wifi.ino](file:///home/diego_silva/Documentos/arduino/arduino_integration/ultrasonic_sensor_wifi/ultrasonic_sensor_wifi.ino) na Arduino IDE.
2. Altere as seguintes constantes no topo do arquivo:
   * `ssid`: Nome da sua rede sem fio.
   * `password`: Senha da sua rede sem fio.
   * `serverUrl`: Substitua `<IP_DO_SEU_SERVIDOR>` pelo endereço IP da máquina rodando o NexusControl na sua rede (ex: `http://192.168.1.100:8080/api/v1/sensors/6/reading/`). Certifique-se de ajustar o ID do sensor no final da URL com o ID gerado no Passo 1.
   * `tokenUrl`: Substitua o IP correspondente para a rota de obtenção do token JWT (ex: `http://192.168.1.100:8080/api/token/`).
3. Compile e carregue o código na sua placa.
4. Abra o Monitor Serial a **115200 bps** para acompanhar as leituras e requisições HTTP POST.

---

### Opção B: Conexão via Gateway Serial (Recomendada para Arduino Uno/Mega)

Se você estiver usando um Arduino Uno comum conectado via cabo USB ao computador hospedeiro:

1. Carregue o arquivo [ultrasonic_sensor_serial.ino](file:///home/diego_silva/Documentos/arduino/arduino_integration/ultrasonic_sensor_serial/ultrasonic_sensor_serial.ino) no seu Arduino usando a Arduino IDE.
2. Mantenha o Arduino conectado à porta USB.
3. Instale a biblioteca do Python necessária para ler a porta USB:
   ```bash
   pip install pyserial requests
   ```
4. Abra o arquivo [serial_gateway.py](file:///home/diego_silva/Documentos/arduino/arduino_integration/serial_gateway.py) e altere a constante `SENSOR_ID` com o número gerado no Passo 1.
5. Verifique a porta USB onde seu Arduino está conectado. No Linux, costuma ser `/dev/ttyACM0` ou `/dev/ttyUSB0`. Ajuste `SERIAL_PORT` no script se necessário.
6. Execute o gateway:
   ```bash
   python arduino_integration/serial_gateway.py
   ```
7. O script irá ler a porta serial a cada 2 segundos, buscar o token JWT no Django e postar os dados de distância na API automaticamente na porta **8080**.

---

## 🎨 Monitorando em Tempo Real

Após iniciar a transmissão (por Wi-Fi ou pelo Gateway Serial), acesse o dashboard do NexusControl em [http://localhost:8080](http://localhost:8080). 
Você verá o novo sensor de distância atualizando o valor em tempo real com os efeitos e alertas visuais ativados!
