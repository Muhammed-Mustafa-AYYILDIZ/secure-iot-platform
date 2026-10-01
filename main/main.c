#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "led_strip.h"

// Donanım pin ve adet tanımlamaları
#define LED_STRIP_BLINK_GPIO 48
#define LED_STRIP_LED_NUMBERS 1

// Loglama için terminal etiketi
static const char *TAG = "SECURE_IOT";

// LED kontrol nesnesi (RMT sürücüsü tarafından yönetilir)
static led_strip_handle_t led_strip;

// Görevler arası veri iletim kuyruğu handle'ı
static QueueHandle_t s_led_count_queue = NULL;

/* 
 * Donanım Katmanı Yapılandırması:
 * ESP32-S3 dahili RMT modülü üzerinden LED Strip ayarlanır.
 */
static void configure_led(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,
        .max_leds = LED_STRIP_LED_NUMBERS,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz çözünürlük
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

/*
 * Görev 1: Kuyruktan Veri Okuma ve Loglama (Receiver Task)
 */
void simple_log_task(void *pvParameters)
{
    uint32_t received_count = 0;

    while (1) {
        // Kuyrukta veri yoksa CPU harcamadan sonsuza kadar uyur (Blocked)
        if (xQueueReceive(s_led_count_queue, &received_count, portMAX_DELAY) == pdPASS) {
            ESP_LOGI(TAG, "Kuyruktan veri alindi! LED tetiklenme sayisi: %lu", received_count);
        }
    }
}

/*
 * Görev 2: LED Yakıp Söndürme ve Sayacı Kuyruğa Yollama (Sender Task)
 */
void led_blink_task(void *pvParameters)
{
    uint8_t s_led_state = 0;
    uint32_t blink_counter = 0;

    while (1) {
        if (s_led_state) {
            // Kırmızı renk yak
            led_strip_set_pixel(led_strip, 0, 20, 0, 0);
            led_strip_refresh(led_strip);

            // Sayacı artır ve kuyruğa yolla
            blink_counter++;
            xQueueSend(s_led_count_queue, &blink_counter, 0);
        } else {
            // LED'i söndür
            led_strip_clear(led_strip);
        }

        s_led_state = !s_led_state;
        vTaskDelay(pdMS_TO_TICKS(500)); // 500 ms bekle
    }
}

/*
 * Ana Giriş Noktası (app_main)
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Donanim yapilandiriliyor...");
    configure_led();

    // 5 eleman kapasiteli, uint32_t tipinde thread-safe kuyruk oluşturuluyor
    s_led_count_queue = xQueueCreate(5, sizeof(uint32_t));
    if (s_led_count_queue == NULL) {
        ESP_LOGE(TAG, "Kuyruk olusturulamadi! Bellek yetersiz.");
        return;
    }

    ESP_LOGI(TAG, "Task'lar baslatiliyor...");

    // Task 1: Tüketici (Receiver)
    xTaskCreate(simple_log_task, "log_task", 2048, NULL, 1, NULL);

    // Task 2: Üretici (Sender)
    xTaskCreate(led_blink_task, "led_task", 2048, NULL, 1, NULL);
}