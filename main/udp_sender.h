#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "credentials.h"

// Telemetri veri yapısı
typedef struct {
    float temperature;
    float pressure;
    uint32_t timestamp_ms;
} sensor_data_t;

// Protokol Sabitleri
#define PROTOCOL_MAGIC_BYTE   0x5A
#define PROTOCOL_VERSION      0x01

#define IV_SIZE               12  // AES-GCM standart Nonce boyutu
#define PAYLOAD_SIZE          12  // Şifrelenecek sensör yükü
#define TAG_SIZE              16  // AES-GCM kimlik doğrulama etiketi
#define PACKET_TOTAL_SIZE     (IV_SIZE + PAYLOAD_SIZE + TAG_SIZE)  // 40 Bayt

void udp_sender_init(QueueHandle_t udp_queue);