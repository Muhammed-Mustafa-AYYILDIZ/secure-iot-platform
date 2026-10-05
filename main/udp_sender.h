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

#define PAYLOAD_SIZE          12
#define HMAC_SIZE             32
#define PACKET_TOTAL_SIZE     (PAYLOAD_SIZE + HMAC_SIZE)   // 44

void udp_sender_init(QueueHandle_t udp_queue);