import socket
import struct
import hmac
import hashlib
import time
import os
import re
import sys

# credentials.h dosyasından ayarları dinamik okuma
def load_credentials_from_header(header_path):
    creds = {}
    if not os.path.exists(header_path):
        return creds
    with open(header_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            # #define KEY "VALUE" veya #define KEY VALUE formatını yakalar
            match = re.match(r'#define\s+(\w+)\s+"?([^"]+)"?', line)
            if match:
                creds[match.group(1)] = match.group(2)
    return creds

# main/credentials.h dosyasını ara (aynı dizinde veya main/ altında)
possible_paths = [
    "credentials.h",
    os.path.join("main", "credentials.h"),
    os.path.join("..", "main", "credentials.h")
]

config = {}
for p in possible_paths:
    if os.path.exists(p):
        config = load_credentials_from_header(p)
        print(f"[BİLGİ] Yapılandırma yüklendi: {p}")
        break

TARGET_IP = config.get("DEST_IP_ADDR")
TARGET_PORT = int(config.get("DEST_PORT", 12345))
PSK_STR = config.get("PSK_STR")

# credentials.h bulunamazsa veya eksikse komut satırından fallback
if not TARGET_IP or not PSK_STR:
    if len(sys.argv) >= 3:
        TARGET_IP = sys.argv[1]
        PSK_STR = sys.argv[2]
        print("[BİLGİ] Yapılandırma komut satırı argümanlarından alındı.")
    else:
        print("[HATA] credentials.h bulunamadı ve komut satırı argümanı girilmedi!")
        print("Kullanım: python attacker_simulator.py <HEDEF_IP> <GIZLI_ANAHTAR>")
        sys.exit(1)

CORRECT_PSK = PSK_STR.encode("utf-8")
WRONG_PSK = b"YanlisAnahtar9999"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def send_packet(payload, key, label):
    signature = hmac.new(key, payload, hashlib.sha256).digest()
    packet = payload + signature
    sock.sendto(packet, (TARGET_IP, TARGET_PORT))
    print(f"[SALDIRI GÖNDERİLDİ] {label}")

print(f"--- SALDIRI SENARYOLARI BAŞLATILIYOR (Hedef: {TARGET_IP}:{TARGET_PORT}) ---\n")

# Senaryo A: Yanlış Anahtarla Üretilmiş İmza (Sahte Cihaz Girişimi)
# Saldırgan veriyi doğru formatta hazırlar ama PSK'yi bilmediği için yanlış anahtarla imzalar.
payload_fake_key = struct.pack("!BBIhI", 0x5A, 0x01, 100, 2500, 101325)
send_packet(payload_fake_key, WRONG_PSK, "Senaryo A: Yanlış PSK ile üretilmiş HMAC")

time.sleep(2)

# Senaryo B: Ortadaki Adam (MITM) Veri Tahrifatı
# Saldırgan geçerli bir paketi yakalar, sıcaklığı kasıtlı olarak 90°C (9000) yapar ama imzayı güncelleyemez.
valid_payload = struct.pack("!BBIhI", 0x5A, 0x01, 101, 2400, 101325)
valid_hmac = hmac.new(CORRECT_PSK, valid_payload, hashlib.sha256).digest()
tampered_payload = struct.pack("!BBIhI", 0x5A, 0x01, 101, 9000, 101325)
tampered_packet = tampered_payload + valid_hmac
sock.sendto(tampered_packet, (TARGET_IP, TARGET_PORT))
print("[SALDIRI GÖNDERİLDİ] Senaryo B: Verisi tahrif edilmiş (tampered) paket")

time.sleep(2)

# Senaryo C: Replay Attack (Eski Paketi Tekrar Basma)
# Saldırgan daha önce yakaladığı Seq No: 50 olan geçerli bir paketi tekrar yollar.
replay_payload = struct.pack("!BBIhI", 0x5A, 0x01, 50, 2200, 101320)
send_packet(replay_payload, CORRECT_PSK, "Senaryo C: Replay Attack (Geçmiş Sequence No: 50)")

sock.close()
print("\n--- SALDIRI SENARYOLARI TAMAMLANDI ---")