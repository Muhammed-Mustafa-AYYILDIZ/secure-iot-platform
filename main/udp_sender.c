#include "esp_random.h"
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

// PSK'yı PSA anahtar deposuna yükler (bir kez çağrılır)
static psa_key_id_t s_aes_key = 0;

// PSK'yı AES-GCM anahtarı olarak PSA deposuna yükler
static bool aes_key_init(void)
{
    if (psa_crypto_init() != PSA_SUCCESS) {
        return false;
    }

    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attr, strlen(PSK_STR) * 8);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_GCM);

    psa_status_t st = psa_import_key(&attr,
                                     (const uint8_t *)PSK_STR, strlen(PSK_STR),
                                     &s_aes_key);
    psa_reset_key_attributes(&attr);
    return st == PSA_SUCCESS;
}

static bool encrypt_payload(const uint8_t *iv, const uint8_t *plaintext, size_t plain_len, uint8_t *out)
{
    size_t out_len = 0;
    psa_status_t st = psa_aead_encrypt(
        s_aes_key,
        PSA_ALG_GCM,
        iv, IV_SIZE,
        NULL, 0,
        plaintext, plain_len,
        out, plain_len + TAG_SIZE,
        &out_len
    );

    return (st == PSA_SUCCESS && out_len == (plain_len + TAG_SIZE));
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
    if (!aes_key_init()) {
        ESP_LOGE(TAG, "AES anahtari yuklenemedi!");
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

            // 4. Şifrelencek 12 baytlık ham veriyi geçici bir dizide topla 
            uint8_t plaintext[PAYLOAD_SIZE];
            plaintext[0] = PROTOCOL_MAGIC_BYTE;
            plaintext[1] = PROTOCOL_VERSION;
            memcpy(&plaintext[2], &net_seq, 4);
            memcpy(&plaintext[6], &net_temp, 2);
            memcpy(&plaintext[8], &net_press, 4);

            // 5. 12 baytlık taze IV üretip paletin başına koyuyorum [0..11]
            esp_fill_random(packet_buffer, IV_SIZE);
            // 6. Plaintext'i şifreleyip IV'nin hemen altına yazıyorum [12..39] (12 baytı ciphertextten 16 bayt tagdan)
            if (!encrypt_payload(packet_buffer, plaintext, PAYLOAD_SIZE, packet_buffer + IV_SIZE)){
                ESP_LOGE(TAG, "AES-GCM Sifreleme basarisiz!");
                continue;
            }
            //7. 40 baytık paketi gönderiyorum (12 IV + 12 Ciphertext + 16 Tag olacak şekilde)
            int err = sendto(sock, packet_buffer, PACKET_TOTAL_SIZE, 0,
                             (struct sockaddr *)&dest_addr, sizeof(dest_addr));

            if (err < 0) {
                ESP_LOGE(TAG, "Gonderim hatasi! Hata kodu: %d", errno);
            } else {
                ESP_LOGI(TAG, "AES-GCM Sifreli Paket Yollandi  -> Seq: %lu | T: %.2f C | P: %.1f hPa",
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