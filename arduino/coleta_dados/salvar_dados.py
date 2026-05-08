"""
salvar_dados.py
─────────────────────────────────────────────────────────────
Lê os dados do Arduino Nano 33 BLE Sense via Serial e salva
automaticamente em arquivos CSV separados por gesto.

Uso:
  python salvar_dados.py --port /dev/ttyACM0 --gesto punch
  python salvar_dados.py --port /dev/ttyACM0 --gesto flex
  python salvar_dados.py --port /dev/ttyACM0 --gesto idle

Requisitos:
  pip install pyserial

Após rodar o script, pressione no Monitor Serial (ou envie
pela linha abaixo) a tecla correspondente ao gesto:
  p = punch | f = flex | i = idle

Os dados são salvos em:
  dados/punch.csv
  dados/flex.csv
  dados/idle.csv
─────────────────────────────────────────────────────────────
"""

import argparse
import os
import time
import serial
import serial.tools.list_ports

# Argumentos CLI 
parser = argparse.ArgumentParser(description='Coleta dados do Arduino para CSV')
parser.add_argument('--port',    default=None,    help='Porta serial (ex: /dev/ttyACM0 ou COM3)')
parser.add_argument('--baud',    default=115200,  type=int, help='Baud rate (padrão: 115200)')
parser.add_argument('--gesto',   default=None,    help='Gesto a coletar: punch, flex ou idle')
parser.add_argument('--amostras',default=10,      type=int, help='Quantas janelas coletar (padrão: 10)')
parser.add_argument('--outdir',  default='dados', help='Pasta de saída (padrão: dados/)')
args = parser.parse_args()

GESTO_CMD = {'punch': b'p', 'flex': b'f', 'idle': b'i'}

# Detectar porta automaticamente
def detectar_porta():
    portas = list(serial.tools.list_ports.comports())
    for p in portas:
        if 'ttyACM' in p.device or 'ttyUSB' in p.device or 'usbmodem' in p.device.lower():
            return p.device
    if portas:
        return portas[0].device
    return None

porta = args.port or detectar_porta()
if not porta:
    print("ERRO: Nenhuma porta serial encontrada.")
    print("Portas disponíveis:")
    for p in serial.tools.list_ports.comports():
        print(f"  {p.device}  —  {p.description}")
    exit(1)

# Selecionar gesto
gesto = args.gesto
if not gesto:
    print("Gestos disponíveis: punch, flex, idle")
    gesto = input("Qual gesto deseja coletar? ").strip().lower()

if gesto not in GESTO_CMD:
    print(f"ERRO: gesto '{gesto}' inválido. Use: punch, flex ou idle")
    exit(1)

# Preparar arquivo CSV
os.makedirs(args.outdir, exist_ok=True)
csv_path = os.path.join(args.outdir, f'{gesto}.csv')

# Adiciona ao arquivo existente (append) para acumular amostras
file_mode = 'a' if os.path.exists(csv_path) else 'w'
existing_lines = 0
if file_mode == 'a':
    with open(csv_path, 'r') as f:
        existing_lines = sum(1 for _ in f)

print(f"\n{'='*50}")
print(f"  Coleta de dados — TinyML")
print(f"{'='*50}")
print(f"  Porta:   {porta}  ({args.baud} baud)")
print(f"  Gesto:   {gesto}")
print(f"  Janelas: {args.amostras}")
print(f"  Arquivo: {csv_path}  ({'novo' if file_mode == 'w' else f'append — {existing_lines} linhas existentes'})")
print(f"{'='*50}\n")

# Conectar 
try:
    ser = serial.Serial(porta, args.baud, timeout=5)
    time.sleep(2)  # aguarda reset do Arduino
    ser.reset_input_buffer()
    print(f"✓ Conectado em {porta}\n")
except serial.SerialException as e:
    print(f"ERRO ao abrir porta: {e}")
    exit(1)

# Loop de coleta 
janelas_coletadas = 0
header_written    = (file_mode == 'a')  # se append, header já existe

with open(csv_path, file_mode, newline='') as csv_file:
    while janelas_coletadas < args.amostras:
        print(f"[{janelas_coletadas+1}/{args.amostras}] "
              f"Preparando gesto '{gesto}'... pressione ENTER quando pronto.")
        input()

        # Envia comando para o Arduino
        ser.write(GESTO_CMD[gesto])
        print(f"  ▶ Gravando — realize o gesto agora!")

        linhas_janela = 0
        in_window     = False
        buffer_lines  = []

        timeout_start = time.time()
        while time.time() - timeout_start < 10:  # timeout 10s
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode('utf-8', errors='ignore').strip()

            if line.startswith('# INICIO_GESTO'):
                in_window    = True
                buffer_lines = []
                continue

            if line.startswith('aX,aY,aZ'):  # header CSV
                if not header_written:
                    csv_file.write(line + '\n')
                    header_written = True
                continue

            if line.startswith('# FIM_GESTO'):
                in_window = False
                # Escreve buffer no arquivo
                for l in buffer_lines:
                    csv_file.write(l + '\n')
                csv_file.flush()
                janelas_coletadas += 1
                print(f"  ✓ Janela salva ({linhas_janela} amostras)\n")
                break

            if in_window and ',' in line:
                buffer_lines.append(line)
                linhas_janela += 1
        else:
            print("  ⚠ Timeout — nenhum dado recebido nesta janela.")


print(f" Coleta finalizada!")
print(f"  {janelas_coletadas} janelas salvas em: {csv_path}")
ser.close()
