import socket
import struct
import hmac
import hashlib

# ESP32 ile aynı olacak ortak gizli anahtarımız
PSK = b"GizliAnahtar12345" 
last_seq_no = -1

# UDP Soketi Oluşturma
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# 0.0.0.0 diyerek bilgisayarın tüm ağ kartlarını dinliyoruz
sock.bind(("0.0.0.0", 3333)) 

print("[*] GUVENLI ALICI BASLATILDI (Port dinleniyor)...")

while True:
    data, addr = sock.recvfrom(1024)

    # 1. KONTROL: Boyut Kontrolü
    # 12 bayt veri + 32 bayt HMAC = 44 bayt olmalı.
    if len(data) != 44:
        print(f"[-] HATALI BOYUT: {len(data)} bayt geldi (Beklenen: 44). Paket atildi.")
        continue

    payload = data[:12]        # İlk 12 bayt: Sensör verileri
    received_hmac = data[12:]  # Son 32 bayt: ESP32'nin bastığı mühür (imza)

    # 2. KONTROL: Bütünlük ve Kimlik Doğrulama (HMAC)
    # Biz de elimizdeki PSK ile bu 12 baytın imzasını hesaplıyoruz
    expected_hmac = hmac.new(PSK, payload, hashlib.sha256).digest()
    
    # İki imza birebir aynı mı?
    if not hmac.compare_digest(expected_hmac, received_hmac):
        print(f"[!] GUVENLIK IHLALI: Imza tutmuyor! Sahte paket! Gonderen: {addr[0]}")
        continue

    # 3. VERİYİ ÇÖZME: İmza doğruysa paketi güvenle açabiliriz
    magic, version, seq_no, temp_raw, press_raw = struct.unpack("!BBIhI", payload)

    # 4. KONTROL: Replay Attack (Eski paketleri tekrar yollama saldırısı)
    if seq_no <= last_seq_no:
        print(f"[!] REPLAY ATTACK: Paket sirasi eski! Beklenen > {last_seq_no}, Gelen: {seq_no}")
        continue

    # Her şey geçerli ise sıra numarasını güncelle ve ekrana yaz
    last_seq_no = seq_no
    temp = temp_raw / 100.0
    press = press_raw / 100.0

    print(f"[+] GUVENLI Paket #{seq_no:<4} | Gonderen: {addr[0]} | Sicaklik: {temp:6.2f} °C | Basinc: {press:7.2f} hPa")