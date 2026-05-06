#include <Arduino_OV767X.h>

// QQVGA (160x120) = menos dados, maior FPS que QCIF (176x144)
#define FRAME_WIDTH  160
#define FRAME_HEIGHT 120

// Marcador de inicio de frame para sincronismo
const byte MARKER[] = {0xFF, 0xAA, 0xFF, 0xAA};

byte frame[FRAME_WIDTH * FRAME_HEIGHT];

void setup() {
  Serial.begin(921600);
  while (!Serial);

  if (!Camera.begin(QQVGA, GRAYSCALE, 1)) {
    Serial.println("Falha ao iniciar a camera!");
    while (1);
  }
}

void loop() {
  Camera.readFrame(frame);

  // Envia marcador + frame (sem delay!)
  Serial.write(MARKER, sizeof(MARKER));
  Serial.write(frame, sizeof(frame));
}