/*
 * inferencia_gestos.ino
 * Arduino Nano 33 BLE Sense — TinyML Parte 1
 *
 * Executa inferência de classificação de gestos em tempo real
 * usando TensorFlow Lite Micro com o modelo int8 gerado pelo
 * notebook 02_conversao_tflite.ipynb.
 *
 * Classes reconhecidas:
 *   0 = punch  (soco rápido para frente)
 *   1 = flex   (dobrar pulso para cima)
 *   2 = idle   (parado)
 *
 * Dependências (Arduino Library Manager):
 *   - Arduino_LSM9DS1           (v1.1.0+)
 *   - Arduino_TensorFlowLite    (v2.4.0-ALPHA ou superior)
 *     → buscar por "TensorFlow Lite" na Library Manager
 *
 * Arquivos necessários na mesma pasta:
 *   - model_data.h    (gerado pelo notebook 02)
 *   - norm_params.h   (gerado pelo notebook 02)
 *
 */

// TensorFlow Lite Micro
#include <TensorFlowLite.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// Modelo e parâmetros gerados pelo Python 
#include "model_data.h"
#include "norm_params.h"

// IMU
#include <Arduino_LSM9DS1.h>

// Configurações 
constexpr float  kDetectionThreshold = 2.5f;  // g — gatilho de movimento
constexpr float  kConfidenceMin      = 0.70f;  // mínimo p/ aceitar predição
constexpr int    kTensorArenaSize    = 32 * 1024;  // 32 KB de arena

// LEDs (active LOW)
constexpr int LED_R = 22;
constexpr int LED_G = 23;
constexpr int LED_B = 24;

// Variáveis TFLite
namespace {
  tflite::MicroErrorReporter   micro_error_reporter;
  tflite::ErrorReporter*       error_reporter = &micro_error_reporter;
  tflite::AllOpsResolver       resolver;
  const tflite::Model*         model      = nullptr;
  tflite::MicroInterpreter*    interpreter = nullptr;
  TfLiteTensor*                input      = nullptr;
  TfLiteTensor*                output     = nullptr;
  uint8_t tensor_arena[kTensorArenaSize];
}

// Buffer de amostras IMU
float imu_buffer[kNumSamples * kNumFeatures];  // 119 × 6 = 714 floats


void setup() {
  Serial.begin(115200);

  // LEDs
  pinMode(LED_R, OUTPUT); pinMode(LED_G, OUTPUT); pinMode(LED_B, OUTPUT);
  setLED(0, 0, 1);  // azul = inicializando

  // Inicializar IMU 
  if (!IMU.begin()) {
    Serial.println("[ERRO] IMU nao inicializado!");
    blinkError();
  }

  // Carregar modelo TFLite 
  model = tflite::GetModel(g_model_data);
  if (model == nullptr) {
    Serial.println("[ERRO] Falha ao carregar modelo TFLite!");
    blinkError();
  }

  // Criar interpretador
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("[ERRO] Falha ao alocar tensors!");
    blinkError();
  }

  input  = interpreter->input(0);
  output = interpreter->output(0);

  // Informações no Serial 
  Serial.println("  TinyML — Classificador de Gestos");
  Serial.println("  Arduino Nano 33 BLE Sense");
  Serial.print  ("  Modelo:      "); Serial.print(g_model_data_len); Serial.println(" bytes");
  Serial.print  ("  Arena:       "); Serial.print(kTensorArenaSize / 1024); Serial.println(" KB");
  Serial.print  ("  Input dim:   "); Serial.println(input->dims->data[1]);
  Serial.print  ("  Classes:     ");
  for (int i = 0; i < kNumClasses; i++) {
    Serial.print(kClassLabels[i]);
    if (i < kNumClasses - 1) Serial.print(", ");
  }
  Serial.println();
  Serial.println("Realizando gestos — aguardando...");
  Serial.println();

  setLED(0, 1, 0);  // verde = pronto
  delay(500);
  setLED(0, 0, 0);
}


void loop() {
  float aX, aY, aZ, gX, gY, gZ;

  // Aguarda movimento significativo
  while (true) {
    if (IMU.accelerationAvailable()) {
      IMU.readAcceleration(aX, aY, aZ);
      float mag = sqrt(aX*aX + aY*aY + aZ*aZ);
      if (mag > kDetectionThreshold) break;
    }
  }

  // Coleta janela de kNumSamples amostras 
  setLED(1, 0, 0);  // vermelho = coletando

  int idx = 0;
  while (idx < kNumSamples) {
    if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
      IMU.readAcceleration(aX, aY, aZ);
      IMU.readGyroscope   (gX, gY, gZ);

      imu_buffer[idx * kNumFeatures + 0] = aX;
      imu_buffer[idx * kNumFeatures + 1] = aY;
      imu_buffer[idx * kNumFeatures + 2] = aZ;
      imu_buffer[idx * kNumFeatures + 3] = gX;
      imu_buffer[idx * kNumFeatures + 4] = gY;
      imu_buffer[idx * kNumFeatures + 5] = gZ;
      idx++;
    }
  }

  // Normalização (mean/std por feature) 
  // Aplica os mesmos parâmetros de normalização do treinamento
  for (int s = 0; s < kNumSamples; s++) {
    for (int f = 0; f < kNumFeatures; f++) {
      imu_buffer[s * kNumFeatures + f] =
          (imu_buffer[s * kNumFeatures + f] - kNormMean[f]) / kNormStd[f];
    }
  }

  // Preenche tensor de entrada 
  // O modelo espera input shape (1, 714) = (1, 119*6)
  TfLiteAffineQuantization* quant = (TfLiteAffineQuantization*)input->quantization.params;
  float input_scale    = input->params.scale;
  int32_t input_zp     = input->params.zero_point;

  for (int i = 0; i < kNumSamples * kNumFeatures; i++) {
    // Quantiza de float32 para int8
    int32_t val = (int32_t)(imu_buffer[i] / input_scale) + input_zp;
    val = constrain(val, -128, 127);
    input->data.int8[i] = (int8_t)val;
  }

  // Inferência
  setLED(0, 0, 1);  // azul = inferindo

  unsigned long t0 = micros();
  TfLiteStatus status = interpreter->Invoke();
  unsigned long inferTime = micros() - t0;

  if (status != kTfLiteOk) {
    Serial.println("[ERRO] Falha na invocação do modelo!");
    setLED(1, 0, 0);
    delay(500);
    return;
  }

  // Dequantiza saída e encontra classe
  float output_scale = output->params.scale;
  int32_t output_zp  = output->params.zero_point;

  float scores[kNumClasses];
  int   best_class    = 0;
  float best_score    = -1.0f;

  for (int i = 0; i < kNumClasses; i++) {
    scores[i] = (output->data.int8[i] - output_zp) * output_scale;
    if (scores[i] > best_score) {
      best_score  = scores[i];
      best_class  = i;
    }
  }

  // Exibe resultado
  Serial.print("→ Gesto: ");
  if (best_score >= kConfidenceMin) {
    Serial.print(kClassLabels[best_class]);
    Serial.print("  (");
    Serial.print(best_score * 100.0f, 1);
    Serial.print("%)");

    // LED por classe
    if (best_class == 0)      setLED(1, 0, 0);  // punch = vermelho
    else if (best_class == 1) setLED(0, 1, 0);  // flex  = verde
    else                      setLED(0, 0, 1);  // idle  = azul
  } else {
    Serial.print("incerto (");
    Serial.print(best_score * 100.0f, 1);
    Serial.print("% < limiar ");
    Serial.print(kConfidenceMin * 100.0f, 0);
    Serial.print("%)");
    setLED(1, 1, 0);  // amarelo = incerto
  }

  Serial.print("  |  tempo de inferência: ");
  Serial.print(inferTime / 1000.0f, 2);
  Serial.println(" ms");

  // Detalha scores de todas as classes
  Serial.print("   Scores: ");
  for (int i = 0; i < kNumClasses; i++) {
    Serial.print(kClassLabels[i]);
    Serial.print("=");
    Serial.print(scores[i] * 100.0f, 1);
    Serial.print("%  ");
  }
  Serial.println();

  delay(500);
  setLED(0, 0, 0);
  delay(200);
}


// Pisca LED vermelho indefinidamente em caso de erro fatal
void blinkError() {
  while (true) {
    setLED(1, 0, 0); delay(200);
    setLED(0, 0, 0); delay(200);
  }
}

void setLED(int r, int g, int b) {
  digitalWrite(LED_R, r ? LOW : HIGH);
  digitalWrite(LED_G, g ? LOW : HIGH);
  digitalWrite(LED_B, b ? LOW : HIGH);
}
