#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "wifi_app.h"

#include "lcd1602.h"
#include "bmp280.h"

#define I2C_SDA_PIN GPIO_NUM_8
#define I2C_SCL_PIN GPIO_NUM_9

static const char *TAG = "APP_ORCHESTRATOR";

typedef struct {
    float temperature;
    float pressure;
    uint32_t timestamp_ms;
} sensor_data_t;

typedef struct {
    i2c_master_dev_handle_t bmp_handle;
} sensor_task_params_t;

typedef struct {
    i2c_master_dev_handle_t lcd_handle;
} display_task_params_t;

static QueueHandle_t s_sensor_queue = NULL;

static void sensor_task(void *pvParameters)
{
    sensor_task_params_t *params = (sensor_task_params_t *)pvParameters;
    bmp280_data_t raw_bmp;
    sensor_data_t out_data;

    ESP_LOGI(TAG, "Sensor gorevi baslatildi.");

    while (1) {
        if (bmp280_read_data(params->bmp_handle, &raw_bmp) == ESP_OK) {
            out_data.temperature = raw_bmp.temperature;
            out_data.pressure = raw_bmp.pressure;
            out_data.timestamp_ms = pdTICKS_TO_MS(xTaskGetTickCount());

            ESP_LOGI(TAG, "[Sensor Task] [%lu ms] Okundu -> T: %.2f C | P: %.1f hPa",
                     out_data.timestamp_ms, out_data.temperature, out_data.pressure);

            xQueueOverwrite(s_sensor_queue, &out_data);
        } else {
            ESP_LOGE(TAG, "[Sensor Task] Okuma hatasi!");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void display_task(void *pvParameters)
{
    display_task_params_t *params = (display_task_params_t *)pvParameters;
    sensor_data_t received_data;
    char line_buf[17];

    ESP_LOGI(TAG, "Ekran gorevi baslatildi.");

    while (1) {
        if (xQueueReceive(s_sensor_queue, &received_data, portMAX_DELAY) == pdTRUE) {
            snprintf(line_buf, sizeof(line_buf), "Temp:  %.2f C", received_data.temperature);
            lcd_set_cursor(params->lcd_handle, 0, 0);
            lcd_send_string(params->lcd_handle, line_buf);

            snprintf(line_buf, sizeof(line_buf), "Press: %.1f hPa", received_data.pressure);
            lcd_set_cursor(params->lcd_handle, 0, 1);
            lcd_send_string(params->lcd_handle, line_buf);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Sistem baslatiliyor...");
    wifi_init_sta();

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_SCL_PIN,
        .sda_io_num = I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    i2c_device_config_t lcd_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = LCD_ADDR,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t lcd_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &lcd_cfg, &lcd_handle));

    i2c_device_config_t bmp_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BMP280_ADDR,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t bmp_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &bmp_cfg, &bmp_handle));

    ESP_ERROR_CHECK(lcd_init(lcd_handle));
    lcd_set_cursor(lcd_handle, 0, 0);
    lcd_send_string(lcd_handle, "RTOS Basliyor...");

    ESP_ERROR_CHECK(bmp280_init(bmp_handle));

    vTaskDelay(pdMS_TO_TICKS(1000));
    lcd_clear(lcd_handle);

    s_sensor_queue = xQueueCreate(1, sizeof(sensor_data_t));
    if (s_sensor_queue == NULL) {
        ESP_LOGE(TAG, "Kuyruk olusturulamadi!");
        return;
    }

    static sensor_task_params_t sensor_params;
    sensor_params.bmp_handle = bmp_handle;
    xTaskCreate(sensor_task, "sensor_task", 3072, &sensor_params, 5, NULL);

    static display_task_params_t display_params;
    display_params.lcd_handle = lcd_handle;
    xTaskCreate(display_task, "display_task", 3072, &display_params, 4, NULL);
}