#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

// Donanım pin ve adet tanımlamaları
#define LED_STRIP_BLINK_GPIO 48
#define LED_STRIP_LED_NUMBERS 1

// Loglama için terminal etiketi
static const char *TAG = "SECURE_IOT";

// LED kontrol nesnesi (RMT sürücüsü tarafından yönetilir)
static led_strip_handle_t led_strip;

/* 
 * Donanım Katmanı Yapılandırması:
 * ESP32-S3'ün dahili RMT (Remote Control) donanımını kullanarak 
 * WS2812/NeoPixel LED için zamanlama ve pin ayarlarını yapar.
 */
static void configure_led(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,
        .max_leds = LED_STRIP_LED_NUMBERS,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz çözünürlük
        .flags.with_dma = false,           // Tek LED için DMA gerekmez
    };

    // Sürücüyü başlat ve olası hataları kontrol et
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    
    // Açılışta LED'i söndür
    led_strip_clear(led_strip);
}

/*
 * Görev 1: Sistem Canlılık Bildirimi (Heartbeat / Logger Task)
 * Sistemin donmadığını doğrulamak için her 2 saniyede bir log üretir.
 */
void simple_log_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "Heartbeat: Sistem saglikli calisiyor...");
        vTaskDelay(pdMS_TO_TICKS(2000)); // 2000 ms uyku
    }
}

/*
 * Görev 2: LED Yakıp Söndürme (Blink Task)
 * Logger görevinden bağımsız olarak her 500 ms'de bir LED'in durumunu değiştirir.
 */
void led_blink_task(void *pvParameters)
{
    uint8_t s_led_state = 0; // 0: Sönük, 1: Yanık

    while (1) {
        if (s_led_state) {
            // Kırmızı renk (R=20, G=0, B=0)
            led_strip_set_pixel(led_strip, 0, 20, 0, 0);
            led_strip_refresh(led_strip);
        } else {
            led_strip_clear(led_strip);
        }

        s_led_state = !s_led_state; // Durumu tersle
        vTaskDelay(pdMS_TO_TICKS(500)); // 500 ms uyku
    }
}

/*
 * Ana Giriş Noktası (app_main)
 * Donanımı kurar ve FreeRTOS görevlerini Scheduler'a kaydeder.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Donanim yapilandiriliyor...");
    configure_led();

    ESP_LOGI(TAG, "Task'lar baslatiliyor...");

    // Task 1: Logger (Öncelik: 1, Bellek: 2048 Byte)
    xTaskCreate(simple_log_task, "log_task", 2048, NULL, 1, NULL);

    // Task 2: LED Blink (Öncelik: 1, Bellek: 2048 Byte)
    xTaskCreate(led_blink_task, "led_task", 2048, NULL, 1, NULL);
}