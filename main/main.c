#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#define I2C_SDA_PIN         GPIO_NUM_8
#define I2C_SCL_PIN         GPIO_NUM_9
#define BMP280_I2C_ADDR     0x76

// Register Adresleri
#define BMP280_REG_CALIB      0x88
#define BMP280_REG_CTRL_MEAS  0xF4
#define BMP280_REG_TEMP_DATA  0xFA

static const char *TAG = "BMP280_TEMP";

typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
} bmp280_calib_t;

// Bosch telafi formülü ile ham veriyi °C'ye dönüştürme
float bmp280_compensate_temperature(int32_t raw_temp, bmp280_calib_t *calib)
{
    int32_t var1 = ((((raw_temp >> 3) - ((int32_t)calib->dig_T1 << 1))) * ((int32_t)calib->dig_T2)) >> 11;
    int32_t var2 = (((((raw_temp >> 4) - ((int32_t)calib->dig_T1)) * ((raw_temp >> 4) - ((int32_t)calib->dig_T1))) >> 12) * ((int32_t)calib->dig_T3)) >> 14;
    int32_t t_fine = var1 + var2;
    float temp = (t_fine * 5 + 128) >> 8;
    return temp / 100.0f;
}

void app_main(void)
{
    ESP_LOGI(TAG, "I2C Master baslatiliyor...");

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

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BMP280_I2C_ADDR,
        .scl_speed_hz = 100000,
    };

    i2c_master_dev_handle_t bmp280_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &bmp280_handle));

    // 1. Kalibrasyon katsayılarını oku
    uint8_t reg_calib = BMP280_REG_CALIB;
    uint8_t calib_buf[6] = {0};
    ESP_ERROR_CHECK(i2c_master_transmit_receive(bmp280_handle, &reg_calib, 1, calib_buf, 6, 1000));

    bmp280_calib_t calib;
    calib.dig_T1 = (uint16_t)((calib_buf[1] << 8) | calib_buf[0]);
    calib.dig_T2 = (int16_t)((calib_buf[3] << 8) | calib_buf[2]);
    calib.dig_T3 = (int16_t)((calib_buf[5] << 8) | calib_buf[4]);

    // 2. Sensörü başlat: ctrl_meas (0xF4) register'ına 0x27 yaz (Normal Mod, Temp x1, Press x1)
    uint8_t ctrl_cmd[2] = {BMP280_REG_CTRL_MEAS, 0x27};
    ESP_ERROR_CHECK(i2c_master_transmit(bmp280_handle, ctrl_cmd, 2, 1000));
    ESP_LOGI(TAG, "BMP280 Normal moda alindi, olcum basliyor...");

    // İlk ölçümün oturması için kısa bir bekleme
    vTaskDelay(pdMS_TO_TICKS(100));

    // 3. Canlı Ölçüm Döngüsü
    while (1) {
        uint8_t reg_temp = BMP280_REG_TEMP_DATA;
        uint8_t raw_buf[3] = {0};

        // 0xFA, 0xFB, 0xFC adreslerindeki 3 baytı oku
        esp_err_t ret = i2c_master_transmit_receive(bmp280_handle, &reg_temp, 1, raw_buf, 3, 1000);

        if (ret == ESP_OK) {
            // 20-bitlik ham veriyi oluştur
            int32_t raw_temp = (int32_t)((((uint32_t)raw_buf[0]) << 12) |
                                         (((uint32_t)raw_buf[1]) << 4) |
                                         (((uint32_t)raw_buf[2]) >> 4));

            float temperature = bmp280_compensate_temperature(raw_temp, &calib);
            ESP_LOGI(TAG, "Sicaklik: %.2f °C (Ham: %ld)", temperature, raw_temp);
        } else {
            ESP_LOGE(TAG, "Veri okuma hatasi: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 saniyede bir güncelle
    }
}