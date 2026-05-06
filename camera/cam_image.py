import serial
import numpy as np
import cv2
import time

WIDTH      = 160
HEIGHT     = 120
PORT       = '/dev/ttyACM0'  # Linux; use 'COMx' no Windows
BAUD       = 921600
MARKER     = bytes([0xFF, 0xAA, 0xFF, 0xAA])
FRAME_SIZE = WIDTH * HEIGHT

ser = serial.Serial(PORT, BAUD, timeout=5)
ser.reset_input_buffer()

cv2.namedWindow('OV7675', cv2.WINDOW_NORMAL)

fps_time = time.time()
fps_count = 0
fps = 0.0

def find_marker(ser):
    """Sincroniza lendo byte a byte até encontrar o marcador de inicio."""
    buf = b''
    while True:
        buf += ser.read(1)
        if buf[-4:] == MARKER:
            return True
        if len(buf) > 8:
            buf = buf[-4:]  # mantém só os últimos bytes

while True:
    find_marker(ser)
    raw = ser.read(FRAME_SIZE)

    if len(raw) == FRAME_SIZE:
        frame = np.frombuffer(raw, dtype=np.uint8).reshape((HEIGHT, WIDTH)).copy()

        # Calcula e exibe FPS
        fps_count += 1
        elapsed = time.time() - fps_time
        if elapsed >= 1.0:
            fps = fps_count / elapsed
            fps_count = 0
            fps_time = time.time()

        cv2.putText(frame, f'{fps:.1f} FPS', (5, 15),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, 255, 1)
        cv2.imshow('OV7675', frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

ser.close()
cv2.destroyAllWindows()