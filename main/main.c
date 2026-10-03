#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#define I2C_SDA_PIN         GPIO_NUM_8
#define I2C_SCL_PIN         GPIO_NUM_9
#define BMP280_I2C_ADDR     0x76
#define BMP280_REG_CHIP_ID  0xD0
#define BMP280_CHIP_ID      0x58

static const char *TAG = "BMP280_TEST";

void app_main(void)
{
    ESP_LOGI(TAG, "I2C Master baslatiliyor...");

    // 1. I2C Bus Kurulumu
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_SCL_PIN,
        .sda_io_num = I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    // 2. BMP280 Cihazını Bus'a Ekleme (Device Handle)
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BMP280_I2C_ADDR,
        .scl_speed_hz = 100000, // 100 kHz Standart Mod
    };

    i2c_master_dev_handle_t bmp280_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &bmp280_handle));

    // 3. Chip ID Okuma
    uint8_t reg_addr = BMP280_REG_CHIP_ID;
    uint8_t chip_id = 0;

    // Önce 0xD0 yazılır, hemen ardından gelen 1 bayt cevap okunur
    esp_err_t ret = i2c_master_transmit_receive(bmp280_handle, &reg_addr, 1, &chip_id, 1, 1000);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Okunan Chip ID: 0x%02X", chip_id);
        if (chip_id == BMP280_CHIP_ID) {
            ESP_LOGI(TAG, "Dogrulama basarili! Gecerli bir BMP280 bagli.");
        } else {
            ESP_LOGW(TAG, "Beklenmeyen Chip ID! (Beklenen: 0x%02X)", BMP280_CHIP_ID);
        }
    } else {
        ESP_LOGE(TAG, "I2C iletisim hatasi: %s", esp_err_to_name(ret));
    }
}