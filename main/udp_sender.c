#include "udp_sender.h"
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "psa/crypto.h"

static const char *TAG = "UDP_SENDER";
static QueueHandle_t s_udp_queue = NULL;
static psa_key_id_t s_hmac_key = 0;

// PSK'yı PSA anahtar deposuna yükler (bir kez çağrılır)
static bool hmac_key_init(void)
{
    if (psa_crypto_init() != PSA_SUCCESS) {
        return false;
    }

    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attr, strlen(PSK_STR) * 8);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_256));

    psa_status_t st = psa_import_key(&attr,
                                     (const uint8_t *)PSK_STR, strlen(PSK_STR),
                                     &s_hmac_key);
    psa_reset_key_attributes(&attr);
    return st == PSA_SUCCESS;
}

// data'nın HMAC-SHA256 imzasını out'a yazar (32 bayt)
static bool compute_hmac(const uint8_t *data, size_t len, uint8_t *out)
{
    size_t out_len = 0;
    psa_status_t st = psa_mac_compute(s_hmac_key, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                                      data, len, out, HMAC_SIZE, &out_len);
    return (st == PSA_SUCCESS && out_len == HMAC_SIZE);
}

static void udp_sender_task(void *pvParameters)
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(DEST_IP_ADDR);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);

    // 1. IP adresi atanana kadar bekle (Hata 118'i engeller)
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info;
    while (1) {
        if (netif != NULL && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            if (ip_info.ip.addr != 0) {
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Soket acilamadi! Hata: %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // HMAC anahtarını yükle
    if (!hmac_key_init()) {
        ESP_LOGE(TAG, "HMAC anahtari yuklenemedi!");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UDP Soketi hazir. Hedef: %s:%d", DEST_IP_ADDR, DEST_PORT);

    uint8_t packet_buffer[PACKET_TOTAL_SIZE];
    uint32_t seq_num = 0;
    sensor_data_t received_sensor;

    while (1) {
        if (s_udp_queue != NULL && xQueueReceive(s_udp_queue, &received_sensor, portMAX_DELAY) == pdTRUE) {

            // 2. Canlı BMP280 verilerini tamsayıya ölçekle
            int16_t temp_scaled   = (int16_t)(received_sensor.temperature * 100.0f);
            uint32_t press_scaled = (uint32_t)(received_sensor.pressure * 100.0f);

            // 3. Big-Endian dönüşümü
            uint32_t net_seq   = htonl(seq_num);
            uint16_t net_temp  = htons((uint16_t)temp_scaled);
            uint32_t net_press = htonl(press_scaled);

            // 4. 12 baytlık payload
            packet_buffer[0] = PROTOCOL_MAGIC_BYTE;
            packet_buffer[1] = PROTOCOL_VERSION;
            memcpy(&packet_buffer[2], &net_seq, 4);
            memcpy(&packet_buffer[6], &net_temp, 2);
            memcpy(&packet_buffer[8], &net_press, 4);

            // 5. HMAC-SHA256: ilk 12 bayt imzalanır, 32 bayt sona eklenir
            if (!compute_hmac(packet_buffer, PAYLOAD_SIZE, packet_buffer + PAYLOAD_SIZE)) {
                ESP_LOGE(TAG, "HMAC hesaplanamadi!");
                continue;
            }

            int err = sendto(sock, packet_buffer, PACKET_TOTAL_SIZE, 0,
                             (struct sockaddr *)&dest_addr, sizeof(dest_addr));

            if (err < 0) {
                ESP_LOGE(TAG, "Gonderim hatasi! Hata kodu: %d", errno);
            } else {
                ESP_LOGI(TAG, "Imzali paket yollandi -> Seq: %lu | T: %.2f C | P: %.1f hPa",
                         seq_num, received_sensor.temperature, received_sensor.pressure);
                seq_num++;
            }
        }
    }

    close(sock);
    vTaskDelete(NULL);
}

void udp_sender_init(QueueHandle_t udp_queue)
{
    s_udp_queue = udp_queue;
    xTaskCreate(udp_sender_task, "udp_sender_task", 6144, NULL, 4, NULL);
}