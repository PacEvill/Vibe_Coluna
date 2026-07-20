/*
  NexusControl - Wearable Posture Monitor (Serial USB Edition)
  
  Código de firmware otimizado para o sensor de postura.
  - Hardware: Arduino Uno + HC-SR04 + Buzzer + Conexão USB com o PC.
  - Alerta local: Beeps no buzzer se a postura estiver degradada (< limite calibrado).
  - Integração: Envia as médias de distância pela USB Serial para o script Gateway.
*/

#include <Arduino.h>

// =========================================================================
// HARDWARE E PINAGEM (Padrão Wearable Postura)
// =========================================================================
const int trigPin = 9;                     // Trigger do HC-SR04
const int echoPin = 10;                    // Echo do HC-SR04
const int buzzerPin = 8;                   // Pino positivo do Buzzer

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
const long INTERVALO_ENVIO_MS = 2000;       // Envio Serial a cada 2 segundos

// Declaração de funções auxiliares
void calibrar();
float lerDistancia();
void atualizarMediaMovel(float nova_leitura);
void verificarPostura();

void setup() {
  // Inicializa a Serial para comunicação com o Gateway no computador
  Serial.begin(9600);
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  // Inicializa o buffer da média móvel
  for (int i = 0; i < NUM_LEITURAS; i++) {
    leituras[i] = 0.0;
  }

  // Notifica o início na serial
  Serial.println(F("{\"status\":\"inicializando\"}"));

  // Calibração de postura ideal
  calibrar();
}

void loop() {
  unsigned long tempo_atual = millis();

  // 1. Amostragem rápida e cálculo da média móvel
  if (tempo_atual - ultimo_tempo_medicao >= INTERVALO_MEDICAO_MS) {
    ultimo_tempo_medicao = tempo_atual;

    float distancia = lerDistancia();
    // Filtra leituras fora do range do sensor (2cm a 200cm)
    if (distancia > 2.0 && distancia < 200.0) {
      atualizarMediaMovel(distancia);
      verificarPostura();
    }
  }

  // 2. Envio periódico dos dados em formato JSON pela Serial USB
  if (tempo_atual - ultimo_tempo_envio >= INTERVALO_ENVIO_MS) {
    ultimo_tempo_envio = tempo_atual;
    
    if (media_atual > 0) {
      Serial.print(F("{\"distance\":"));
      Serial.print(media_atual, 1);
      Serial.println(F("}"));
    } else {
      Serial.println(F("{\"error\":\"sensor_failure\"}"));
    }
  }
}

// Calibração Inicial: O usuário deve estar ereto
void calibrar() {
  // Beeps de preparação
  for (int i = 0; i < 3; i++) {
    tone(buzzerPin, 1000);
    delay(150);
    noTone(buzzerPin);
    delay(150);
  }

  // Aguarda 3 segundos para estabilização do usuário
  delay(3000);

  float soma = 0;
  int leituras_validas = 0;

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
    
    // Imprime a distância base calibrada
    Serial.print(F("{\"status\":\"calibrado\",\"base_distance\":"));
    Serial.print(distancia_base, 1);
    Serial.println(F("}"));

    // Preenche o buffer com a referência
    for (int i = 0; i < NUM_LEITURAS; i++) {
      atualizarMediaMovel(distancia_base);
    }

    // Sinal sonoro de sucesso
    tone(buzzerPin, 1500);
    delay(800);
    noTone(buzzerPin);
  } else {
    Serial.println(F("{\"status\":\"error\",\"details\":\"calibration_failed\"}"));
    while (1) {
      tone(buzzerPin, 200);
      delay(500);
      noTone(buzzerPin);
      delay(500);
    }
  }
}

// Leitura de pulso do sensor ultrassônico
float lerDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duracao = pulseIn(echoPin, HIGH, 30000);
  if (duracao == 0) return 0.0;

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
}

// Avaliação de postura degradada e atuação do buzzer local
void verificarPostura() {
  if (media_atual < (distancia_base - MARGEM_TOLERANCIA_CM)) {
    // Alerta sonoro contínuo a 300Hz
    tone(buzzerPin, 300);
  } else {
    noTone(buzzerPin);
  }
}
