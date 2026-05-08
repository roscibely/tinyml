/*
 * coleta_dados.ino
 * Arduino Nano 33 BLE Sense — TinyML Parte 1
 *
 * Coleta dados do acelerômetro + giroscópio (LSM9DS1) e
 * os imprime via Serial em formato CSV para salvar no PC.
 *
 * Uso:
 *  1. Faça upload para o Arduino Nano 33 BLE Sense
 *  2. Abra o Monitor Serial (115200 baud)
 *  3. Pressione o botão ou escreva 'p' / 'f' / 'i' para
 *     iniciar gravação do gesto correspondente:
 *       p = punch   f = flex   i = idle
 *  4. Realize o gesto quando o LED piscar
 *  5. Copie o CSV e salve em dados/<gesto>.csv
 *
 * Dependências (Arduino Library Manager):
 *   - Arduino_LSM9DS1  (v1.1.0+)
 *
 */

#include <Arduino_LSM9DS1.h>

// Configurações
const int     NUM_SAMPLES   = 119;   // 119 amostras @ 119 Hz ≈ 1 segundo
const float   IMU_THRESHOLD = 2.5f;  // g — limiar de detecção de movimento
const int     LED_R         = 22;
const int     LED_G         = 23;
const int     LED_B         = 24;

// Variáveis globais 
int   samplesRead   = 0;
bool  collecting    = false;
char  currentGesture = '?';


void setup() {
  Serial.begin(115200);
  while (!Serial);

  // LEDs (active LOW no Nano 33 BLE Sense)
  pinMode(LED_R, OUTPUT); pinMode(LED_G, OUTPUT); pinMode(LED_B, OUTPUT);
  setLED(0, 0, 1);  // azul = aguardando

  if (!IMU.begin()) {
    Serial.println("ERRO: IMU nao inicializado!");
    while (1);
  }

  Serial.println("=== Coletor de Gestos TinyML ===");
  Serial.println("Taxa do acelerometro: " + String(IMU.accelerationSampleRate()) + " Hz");
  Serial.println();
  Serial.println("Comandos:");
  Serial.println("  p  →  coletar gesto PUNCH");
  Serial.println("  f  →  coletar gesto FLEX");
  Serial.println("  i  →  coletar IDLE (parado)");
  Serial.println("  a  →  auto-trigger por movimento");
  Serial.println();
  Serial.println("Aguardando comando...");
}

void loop() {
  // Verificar comando serial 
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'p' || cmd == 'f' || cmd == 'i') {
      currentGesture = cmd;
      startCollection();
    } else if (cmd == 'a') {
      waitForMotion();
    }
  }

  // Coleta ativa 
  if (collecting && IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
    float aX, aY, aZ, gX, gY, gZ;
    IMU.readAcceleration(aX, aY, aZ);
    IMU.readGyroscope(gX, gY, gZ);

    // Imprime linha CSV
    Serial.print(aX, 4); Serial.print(',');
    Serial.print(aY, 4); Serial.print(',');
    Serial.print(aZ, 4); Serial.print(',');
    Serial.print(gX, 4); Serial.print(',');
    Serial.print(gY, 4); Serial.print(',');
    Serial.println(gZ, 4);

    samplesRead++;

    if (samplesRead >= NUM_SAMPLES) {
      finishCollection();
    }
  }
}


void startCollection() {
  samplesRead = 0;
  collecting  = true;

  setLED(1, 0, 0);  // vermelho = gravando
  Serial.println();
  Serial.print("# INICIO_GESTO:");
  Serial.println(gestureLabel(currentGesture));
  Serial.println("aX,aY,aZ,gX,gY,gZ");
}

void finishCollection() {
  collecting = false;
  setLED(0, 1, 0);  // verde = concluído
  Serial.print("# FIM_GESTO:");
  Serial.println(gestureLabel(currentGesture));
  Serial.println();
  delay(300);
  setLED(0, 0, 1);  // azul = aguardando
  Serial.println("Aguardando proximo comando (p/f/i)...");
}

// Aguarda movimento para trigger automático
void waitForMotion() {
  Serial.println("Modo auto: aguardando movimento forte...");
  setLED(0, 0, 1);
  float aX, aY, aZ;
  while (true) {
    if (IMU.accelerationAvailable()) {
      IMU.readAcceleration(aX, aY, aZ);
      float magnitude = sqrt(aX*aX + aY*aY + aZ*aZ);
      if (magnitude > IMU_THRESHOLD) {
        currentGesture = '?';
        Serial.println("# Movimento detectado (auto-trigger)");
        startCollection();
        return;
      }
    }
    if (Serial.available()) break;  // cancela ao receber qualquer byte
  }
}

String gestureLabel(char g) {
  if (g == 'p') return "punch";
  if (g == 'f') return "flex";
  if (g == 'i') return "idle";
  return "unknown";
}

// LEDs active-LOW: valor 0 = acende, 1 = apaga
void setLED(int r, int g, int b) {
  digitalWrite(LED_R, r ? LOW : HIGH);
  digitalWrite(LED_G, g ? LOW : HIGH);
  digitalWrite(LED_B, b ? LOW : HIGH);
}
