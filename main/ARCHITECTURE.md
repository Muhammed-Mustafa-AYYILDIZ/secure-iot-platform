# Secure IoT Telemetry Platform - Mimari Yol Haritası

AŞAMA 4.1: Temel UDP İletişimi (Ham Veri & Soket Yönetimi)
    ↓
AŞAMA 4.2: Özel İkili Paket Çerçevesi (Custom Binary Framing & Serialization)
    ↓
AŞAMA 5  : Wi-Fi Olay Yönetimi & FreeRTOS Kuyruk Mimarisi (Sensor -> Queue -> Network)
    ↓
AŞAMA 6  : Güvenlik Katmanı 1 (HMAC-SHA256 ile Bütünlük + Sequence Number)
    ↓
AŞAMA 7  : Güvenlik Katmanı 2 (AES-GCM / AEAD ile Gizlilik ve Şifreleme)
    ↓
AŞAMA 7.1: Zaman Senkronizasyonu (SNTP) & Dayanıklı Replay Penceresi (Replay Window)
    ↓
AŞAMA 8  : Güvenilir Ağ & Standart IoT Protokolleri (TCP + TLS/mTLS ve MQTT)
    ↓
AŞAMA 9  : Güvenli Anahtar Yönetimi (NVS Şifreleme & Device Provisioning)
    ↓
AŞAMA 10 : Donanım Kök Güveni (ESP32-S3 Secure Boot v2 & Flash Encryption)