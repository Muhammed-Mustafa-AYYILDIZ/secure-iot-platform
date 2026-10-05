import socket
import struct
import time
import re
from pathlib import Path

def load_credentials_from_header():
    header_path = Path("main/credentials.h")
    if not header_path.exists():
        header_path = Path("credentials.h")
    content = header_path.read_text(encoding="utf-8")
    port_match = re.search(r'#define\s+DEST_PORT\s+(\d+)', content)
    return int(port_match.group(1))

def main():
    dest_port = load_credentials_from_header()
    target_addr = ("127.0.0.1", dest_port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    print(f"[*] Saldirgan Simülatoru Baslatildi -> Hedef Port: {dest_port}")
    print("-" * 65)

    # 1. Senaryo: Tamamen Sahte/Rastgele Şifreli Paket (Forged Packet)
    print("\n[TEST 1] Sahte/Rastgele 40 Bayt Paket Gonderiliyor...")
    fake_packet = b"X" * 40
    sock.sendto(fake_packet, target_addr)
    time.sleep(1)

    # 2. Senaryo: Bit-Flipping / Veri Manipulasyonu
    # Saldırgan aradaki şifreli paketi yakaladı ama içeriği okuyamıyor,
    # rastgele 1 bitini değiştirip alıcıya iletiyor.
    print("\n[TEST 2] Manipule Edilmis (Bit-Flipped) Paket Gonderiliyor...")
    # 12B Nonce + 12B Ciphertext + 16B Tag
    captured_packet = bytearray(b"\x01" * 12 + b"\xAA" * 12 + b"\xFF" * 16)
    captured_packet[15] ^= 0x01  # Ciphertext içindeki 1 biti ters çeviriyoruz
    sock.sendto(bytes(captured_packet), target_addr)
    time.sleep(1)

    # 3. Senaryo: Yanlış Boyutlu Paket (Truncated/Malformed Packet)
    print("\n[TEST 3] Eksik Boyutlu (30 Bayt) Paket Gonderiliyor...")
    malformed_packet = b"\x00" * 30
    sock.sendto(malformed_packet, target_addr)
    time.sleep(1)

    print("\n[*] Tum saldiri testleri yollandi. Alıcı loglarini kontrol edin.")

if __name__ == "__main__":
    main()