#include <Arduino_LSM9DS1.h>
#include <Arduino_HTS221.h>
#include <Arduino_LPS22HB.h>
#include <Arduino_APDS9960.h>
#include <PDM.h>

// Buffer do microfone
short micBuffer[256];
volatile int micSamples = 0;

void onPDMdata() {
  micSamples = PDM.available();
  PDM.read(micBuffer, micSamples);
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // IMU (accel + gyro + mag)
  if (!IMU.begin())
    Serial.println("Falha: LSM9DS1");

  // Temperatura e Umidade
  if (!HTS.begin())
    Serial.println("Falha: HTS221");

  // Pressão
  if (!BARO.begin())
    Serial.println("Falha: LPS22HB");

  // Gesto / Cor / Proximidade
  if (!APDS.begin())
    Serial.println("Falha: APDS9960");

  // Microfone
  PDM.onReceive(onPDMdata);
  if (!PDM.begin(1, 16000))  // mono, 16kHz
    Serial.println("Falha: Microfone");

  Serial.println("Todos os sensores iniciados!");
}

void loop() {
  // --- Acelerômetro ---
  if (IMU.accelerationAvailable()) {
    float ax, ay, az;
    IMU.readAcceleration(ax, ay, az);
    Serial.print("Accel (g): ");
    Serial.print(ax); Serial.print(", ");
    Serial.print(ay); Serial.print(", ");
    Serial.println(az);
  }

  // --- Giroscópio ---
  if (IMU.gyroscopeAvailable()) {
    float gx, gy, gz;
    IMU.readGyroscope(gx, gy, gz);
    Serial.print("Gyro (dps): ");
    Serial.print(gx); Serial.print(", ");
    Serial.print(gy); Serial.print(", ");
    Serial.println(gz);
  }

  // --- Magnetômetro ---
  if (IMU.magneticFieldAvailable()) {
    float mx, my, mz;
    IMU.readMagneticField(mx, my, mz);
    Serial.print("Mag (uT): ");
    Serial.print(mx); Serial.print(", ");
    Serial.print(my); Serial.print(", ");
    Serial.println(mz);
  }

  // --- Temperatura e Umidade ---
  float temp = HTS.readTemperature();
  float hum  = HTS.readHumidity();
  Serial.print("Temp: "); Serial.print(temp); Serial.print(" C  |  ");
  Serial.print("Umidade: "); Serial.print(hum); Serial.println(" %");

  // --- Pressão ---
  float pressure = BARO.readPressure();
  Serial.print("Pressao: "); Serial.print(pressure); Serial.println(" kPa");

  // --- Proximidade ---
  if (APDS.proximityAvailable()) {
    int prox = APDS.readProximity();  // 0 = perto, 255 = longe
    Serial.print("Proximidade: "); Serial.println(prox);
  }

  // --- Cor (R, G, B, Ambiente) ---
  if (APDS.colorAvailable()) {
    int r, g, b, a;
    APDS.readColor(r, g, b, a);
    Serial.print("Cor R/G/B/A: ");
    Serial.print(r); Serial.print(", ");
    Serial.print(g); Serial.print(", ");
    Serial.print(b); Serial.print(", ");
    Serial.println(a);
  }

  // --- Gesto ---
  if (APDS.gestureAvailable()) {
    int gesture = APDS.readGesture();
    switch (gesture) {
      case GESTURE_UP:    Serial.println("Gesto: CIMA");    break;
      case GESTURE_DOWN:  Serial.println("Gesto: BAIXO");   break;
      case GESTURE_LEFT:  Serial.println("Gesto: ESQUERDA");break;
      case GESTURE_RIGHT: Serial.println("Gesto: DIREITA"); break;
    }
  }

  // --- Microfone (nível de som) ---
  if (micSamples > 0) {
    long sum = 0;
    int n = micSamples / 2;
    for (int i = 0; i < n; i++) sum += abs(micBuffer[i]);
    Serial.print("Mic (media): "); Serial.println(sum / n);
    micSamples = 0;
  }

  Serial.println("---");
  delay(500);
}