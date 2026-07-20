/*
  NexusControl - Wearable Posture Monitor (Senior Edition)
  
  Código de firmware otimizado para o sensor de postura.
  - Hardware: Arduino Uno + ESP-01S (via SoftwareSerial) + HC-SR04 + Buzzer.
  - Biblioteca de Wi-Fi: WiFiEsp (utiliza o ESP-01S como modem AT).
  - Alerta local: Beeps no buzzer se a postura estiver degradada (< limite calibrado).
  - Integração: Envio automático via HTTP POST para o backend na porta 8080.
*/

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <WiFiEsp.h>
#include <string.h>

// =========================================================================
// CONFIGURAÇÕES DE REDE E SERVIDOR
// =========================================================================
const char* ssid = "Moto_edge_40_Neo";     // Nome do Wi-Fi (Ponto de Acesso)
const char* pass = "123456789";            // Senha do Wi-Fi
const char* serverIp = "192.168.1.11";   // IP do computador rodando o Django
const int serverPort = 8000;               // Porta do Django local (8000)
const int sensorId = 1;                    // ID do sensor no Django (ID 1 para Postura)

// =========================================================================
// HARDWARE E PINAGEM (Padrão Wearable Postura)
// =========================================================================
const int trigPin = 9;                     // Trigger do HC-SR04
const int echoPin = 10;                    // Echo do HC-SR04
const int buzzerPin = 8;                   // Pino positivo do Buzzer

// ESP-01S conectado nas portas digitais 2 (RX) e 3 (TX)
SoftwareSerial espSerial(2, 3); 
WiFiEspClient client;

// =========================================================================
// PARÂMETROS DO MONITOR DE POSTURA
// =========================================================================
float distancia_base = 0.0;                // Calibrada na inicialização (postura ideal)
const int MARGEM_TOLERANCIA_CM = 15;       // Se inclinar mais de 15cm para frente, alerta

// Configuração de Filtro: Média Móvel (Suaviza ruídos físicos de leitura)
const int NUM_LEITURAS = 10;
float leituras[NUM_LEITURAS];
int index_leitura = 0;
float total_leituras = 0;
float media_atual = 0;

// Temporizações Sem Bloqueio (Evita travar a CPU com delay)
unsigned long ultimo_tempo_medicao = 0;
const long INTERVALO_MEDICAO_MS = 250;      // Medição rápida (4x por segundo)

unsigned long ultimo_tempo_envio = 0;
const long INTERVALO_ENVIO_MS = 5000;       // Envio para o servidor a cada 5 segundos

unsigned long ultimo_tentativa_wifi = 0;
const long INTERVALO_RECONEXAO_WIFI = 30000; // Tenta reconectar o Wi-Fi a cada 30s se cair

// Declaração de funções auxiliares
void calibrar();
float lerDistancia();
void atualizarMediaMovel(float nova_leitura);
void verificarPostura();
void tentarReconectarWiFi();
void enviarDadosPostura();
bool aguardarRespostaAT(unsigned long timeoutMs);
bool inicializarESP8266();

void setup() {
  Serial.begin(9600);
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // Inicializa o buffer da média móvel
  for (int i = 0; i < NUM_LEITURAS; i++) {
    leituras[i] = 0.0;
  }

  // Inicializar comunicação serial com o ESP-01S
  Serial.println(F("[Hardware] Inicializando ESP-01S..."));

  if (!inicializarESP8266()) {
    Serial.println(F("[Wi-Fi] ESP-01S sem resposta AT. Verifique firmware AT, TX/RX e alimentação 3.3V."));
  }

  WiFi.init(&espSerial);

  // Tentativa de conexão inicial com o Wi-Fi
  Serial.print(F("[Wi-Fi] Conectando a rede: "));
  Serial.println(ssid);
  
  int status = WiFi.status();
  int tentativas = 0;
  while (status != WL_CONNECTED && tentativas < 8) {
    status = WiFi.begin(ssid, pass);
    if (status != WL_CONNECTED) {
      Serial.println(F("[Wi-Fi] Falha. Tentando novamente em 5s..."));
      delay(5000);
      tentativas++;
    }
  }

  if (status == WL_CONNECTED) {
    Serial.print(F("[Wi-Fi] Conectado! IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("[Wi-Fi] ALERTA: Operando em modo offline (Standalone) por enquanto."));
  }

  // Calibração de postura ideal
  calibrar();
}

bool aguardarRespostaAT(unsigned long timeoutMs) {
  unsigned long inicio = millis();
  char buffer[32];
  size_t pos = 0;

  while (millis() - inicio < timeoutMs) {
    while (espSerial.available() > 0) {
      char c = static_cast<char>(espSerial.read());
      if (pos < sizeof(buffer) - 1) {
        buffer[pos++] = c;
        buffer[pos] = '\0';
      }

      if (strstr(buffer, "OK") != nullptr) {
        return true;
      }
    }
  }

  return false;
}

bool inicializarESP8266() {
  // Tenta dois bauds comuns e registra uma falha clara quando o módulo não responde.
  const unsigned long bauds[] = {115200, 9600};

  for (unsigned int i = 0; i < sizeof(bauds) / sizeof(bauds[0]); i++) {
    espSerial.end();
    espSerial.begin(bauds[i]);

    while (espSerial.available() > 0) {
      espSerial.read();
    }

    espSerial.println(F("AT"));
    if (aguardarRespostaAT(1000)) {
      Serial.print(F("[Wi-Fi] ESP-01S respondeu em "));
      Serial.print(bauds[i]);
      Serial.println(F(" baud."));

      espSerial.println(F("AT+CWMODE=1"));
      delay(200);

      if (bauds[i] != 9600) {
        espSerial.println(F("AT+UART_DEF=9600,8,1,0,0"));
        delay(500);
        espSerial.end();
        espSerial.begin(9600);
      }

      return true;
    }
  }

  return false;
}

void loop() {
  unsigned long tempo_atual = millis();

  // 1. Amostragem rápida e cálculo da média móvel
  if (tempo_atual - ultimo_tempo_medicao >= INTERVALO_MEDICAO_MS) {
    ultimo_tempo_medicao = tempo_atual;

    floa „    distancia = lerDistancia();
    // Filtra leituras fora do range físico do sensor HC-SR04 (2cm a 200cm)
    if (distancia > 2.0 && distancia < 200.0) {
      atualizarMediaMovel(distancia);
      verificarPostura();
    }
  }

  // 2. Transmissão periódica das leituras para a central Django
  if (tempo_atual - ultimo_tempo_envio >= INTERVALO_ENVIO_MS) {
    ultimo_tempo_envio = tempo_atual;
    if (media_atual > 0) {
      enviarDadosPostura();
    }
  }
}

// Calibração Inicial: O usuário deve estar ereto
void calibrar() {
  Serial.println(F("======================================="));
  Serial.println(F("    INICIANDO CALIBRAÇÃO DE POSTURA   "));
  Serial.println(F("   Sente-se de forma ereta e correta   "));
  Serial.println(F("======================================="));

  // Beeps de preparação
  for (int i = 0; i < 3; i++) {
    tone(buzzerPin, 1000);
    delay(150);
    noTone(buzzerPin);
    delay(150);
  }

  // Aguarda 3 segundos para estabilização física do usuário
  delay(3000);

  float soma = 0;
  int leituras_validas = 0;

  Serial.println(F("Lendo posição base (distância ideal)..."));
  for (int i = 0; i < 10; i++) {
    float d = lerDistancia();
    if (d > 2.0 && d < 200.0) {
      soma += d;
      leituras_validas++;
    }
    delay(300);
  }

  if (leituras_validas > 0) {
    distancia_base = soma / leituras_validas;
    Serial.print(F("✔ Calibração concluída! Distância de referência: "));
    Serial.print(distancia_base);
    Serial.println(F(" cm"));

    // Preenche o buffer da média móvel com a referência para evitar saltos iniciais
    for (int i = 0; i < NUM_LEITURAS; i++) {
      atualizarMediaMovel(distancia_base);
    }

    // Sinal sonoro de sucesso (Beep longo agudo)
    tone(buzzerPin, 1500);
    delay(800);
    noTone(buzzerPin);
  } else {
    Serial.println(F("❌ ERRO NA CALIBRAÇÃO: Nenhuma leitura válida. Reinicie o dispositivo."));
    while (1) {
      // Pisca buzzer/led em sinal de erro
      tone(buzzerPin, 200);
      delay(500);
      noTone(buzzerPin);
      delay(500);
    }
  }
}

// Leitura direta de pulsos do sensor ultrassônico
float lerDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Mede o tempo de trânsito da onda sonora. Timeout de 30ms.
  long duracao = pulseIn(echoPin, HIGH, 30000);
  if (duracao == 0) return 0.0;

  // Distância em cm: (Tempo em us * Velocidade do som 0.0343 cm/us) / 2
  return (duracao * 0.0343) / 2.0;
}

// Filtro de Média Móvel (FIFO Buffer)
void atualizarMediaMovel(float nova_leitura) {
  total_leituras = total_leituras - leituras[index_leitura];
  leituras[index_leitura] = nova_leitura;
  total_leituras = total_leituras + leituras[index_leitura];
  
  index_leitura++;
  if (index_leitura >= NUM_LEITURAS) {
    index_leitura = 0;
  }

  media_atual = total_leituras / NUM_LEITURAS;

  Serial.print(F("Distância Instantânea: "));
  Serial.print(nova_leitura);
  Serial.print(F(" cm | Média Móvel: "));
  Serial.print(media_atual);
  Serial.println(F(" cm"));
}

// Avaliação de postura degradada e atuação do buzzer local
void verificarPostura() {
  // Se o usuário se inclinar demais (ficando muito próximo do sensor), aciona o alerta
  if (media_atual < (distancia_base - MARGEM_TOLERANCIA_CM)) {
    Serial.println(F("⚠️ ALERTA: Postura degradada detectada!"));
    // Alerta sonoro contínuo a 300Hz
    tone(buzzerPin, 300);
  } else {
    noTone(buzzerPin);
  }
}

// Tenta restabelecer conexão de forma assíncrona
void tentarReconectarWiFi() {
  unsigned long tempo_atual = millis();
  
  if (tempo_atual - ultimo_tentativa_wifi >= INTERVALO_RECONEXAO_WIFI) {
    ultimo_tentativa_wifi = tempo_atual;
    Serial.println(F("[Wi-Fi] Tentando reconectar de forma assíncrona..."));
    
    noTone(buzzerPin); // Desliga buzzer temporariamente para evitar tons travados
    WiFi.begin(ssid, pass);
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(F("[Wi-Fi] Reconectado!"));
    }
  }
}

// Envia os dados estruturados de postura para a API do NexusControl
void enviarDadosPostura() {
  if (WiFi.status() != WL_CONNECTED) {
    tentarReconectarWiFi();
    return;
  }

  // Prepara a string de payload JSON
  char jsonPayload[48];
  char valStr[10];
  dtostrf(media_atual, 4, 1, valStr); // Formata com 1 casa decimal
  snprintf(jsonPayload, sizeof(jsonPayload), "{\"value\": %s}", valStr);

  Serial.print(F("[API] Conectando ao servidor: "));
  Serial.println(serverIp);

  if (client.connect(serverIp, serverPort)) {
    // Monta a requisição POST HTTP/1.0
    // O endpoint /api/v1/sensors/{id}/reading/ não exige autenticação para facilitar conexões IoT.
    char httpRequest[256];
    snprintf(httpRequest, sizeof(httpRequest), 
      "POST /api/v1/sensors/%d/reading/ HTTP/1.0\r\n"
      "Host: %s:%d\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: %d\r\n"
      "\r\n"
      "%s", 
      sensorId, serverIp, serverPort, strlen(jsonPayload), jsonPayload
    );
    
    // Envia os bytes
    client.print(httpRequest);
    Serial.print(F("[API] Leituras publicadas: "));
    Serial.println(jsonPayload);

    // Esvazia e descarta a resposta HTTP para liberar o buffer do chip ESP8266
    unsigned long tempoEspera = millis();
    while (client.connected() && millis() - tempoEspera < 1500) {
      while (client.available()) {
        client.read(); 
      }
    }
    client.stop();
    Serial.println(F("[API] Conexão encerrada com sucesso."));
  } else {
    Serial.println(F("[API] Erro ao conectar ao servidor Django (Porta ocupada ou IP incorreto)."));
  }
}
