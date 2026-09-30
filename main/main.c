#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "MY_PROJECT";

void simple_log_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "Hello FreeRTOS! İlk task calisiyor.");
        
        // İşlemciyi bloke etmeden 1000 milisaniye (1 saniye) bekle
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

void app_main(void)
{
    
    ESP_LOGI(TAG, "Sistem baslatildi, Task olusturuluyor...");

    // Stack size olarak 2048 byte veriyoruz. Basit bir log için fazlasıyla yeterli.
    xTaskCreate(simple_log_task, "log_task", 2048, NULL, 1, NULL);
}