#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#define I2C_SDA_PIN         GPIO_NUM_8
#define I2C_SCL_PIN         GPIO_NUM_9
#define BMP280_I2C_ADDR     0x76

#define BMP280_REG_CALIB      0x88
#define BMP280_REG_CTRL_MEAS  0xF4
#define BMP280_REG_DATA_BURST 0xF7 // 0xF7'den 0xFC'ye 6 bayt (Basınç + Sıcaklık)

static const char *TAG = "BMP280";

typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
} bmp280_calib_t;

// Sıcaklık kompanzasyonu (t_fine çıktısı verir)
float bmp280_compensate_temperature(int32_t raw_temp, bmp280_calib_t *calib, int32_t *t_fine)
{
    int32_t var1 = ((((raw_temp >> 3) - ((int32_t)calib->dig_T1 << 1))) * ((int32_t)calib->dig_T2)) >> 11;
    int32_t var2 = (((((raw_temp >> 4) - ((int32_t)calib->dig_T1)) * ((raw_temp >> 4) - ((int32_t)calib->dig_T1))) >> 12) * ((int32_t)calib->dig_T3)) >> 14;
    *t_fine = var1 + var2;
    float temp = (*t_fine * 5 + 128) >> 8;
    return temp / 100.0f;
}

// Basınç kompanzasyonu (hPa cinsinden float döner)
float bmp280_compensate_pressure(int32_t raw_press, int32_t t_fine, bmp280_calib_t *calib)
{
    int64_t var1, var2, p;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib->dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib->dig_P5) << 17);
    var2 = var2 + (((int64_t)calib->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib->dig_P3) >> 8) + ((var1 * (int64_t)calib->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib->dig_P1) >> 33;

    if (var1 == 0) {
        return 0.0f;
    }

    p = 1048576 - raw_press;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib->dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)calib->dig_P7) << 4);
    return (float)p / 25600.0f; // Pa -> hPa
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

    // 1. Tüm kalibrasyon katsayılarını tek seferde oku (0x88 - 0xA1: 24 bayt)
    uint8_t reg_calib = BMP280_REG_CALIB;
    uint8_t calib_buf[24] = {0};
    ESP_ERROR_CHECK(i2c_master_transmit_receive(bmp280_handle, &reg_calib, 1, calib_buf, 24, 1000));

    bmp280_calib_t calib;
    calib.dig_T1 = (uint16_t)((calib_buf[1]  << 8) | calib_buf[0]);
    calib.dig_T2 = (int16_t) ((calib_buf[3]  << 8) | calib_buf[2]);
    calib.dig_T3 = (int16_t) ((calib_buf[5]  << 8) | calib_buf[4]);
    calib.dig_P1 = (uint16_t)((calib_buf[7]  << 8) | calib_buf[6]);
    calib.dig_P2 = (int16_t) ((calib_buf[9]  << 8) | calib_buf[8]);
    calib.dig_P3 = (int16_t) ((calib_buf[11] << 8) | calib_buf[10]);
    calib.dig_P4 = (int16_t) ((calib_buf[13] << 8) | calib_buf[12]);
    calib.dig_P5 = (int16_t) ((calib_buf[15] << 8) | calib_buf[14]);
    calib.dig_P6 = (int16_t) ((calib_buf[17] << 8) | calib_buf[16]);
    calib.dig_P7 = (int16_t) ((calib_buf[19] << 8) | calib_buf[18]);
    calib.dig_P8 = (int16_t) ((calib_buf[21] << 8) | calib_buf[20]);
    calib.dig_P9 = (int16_t) ((calib_buf[23] << 8) | calib_buf[22]);

    ESP_LOGI(TAG, "Tum katsayilar alindi. dig_P1: %u, dig_P2: %d", calib.dig_P1, calib.dig_P2);

    // 2. Normal moda al: Temp x1, Press x1, Normal Mod -> 0x27
    uint8_t ctrl_cmd[2] = {BMP280_REG_CTRL_MEAS, 0x27};
    ESP_ERROR_CHECK(i2c_master_transmit(bmp280_handle, ctrl_cmd, 2, 1000));
    ESP_LOGI(TAG, "BMP280 olcume hazir.");

    vTaskDelay(pdMS_TO_TICKS(100));

    // 3. Eşzamanlı okuma döngüsü
    while (1) {
        uint8_t reg_data = BMP280_REG_DATA_BURST;
        uint8_t data_buf[6] = {0};

        // 0xF7'den 0xFC'ye 6 baytı ardışık oku
        esp_err_t ret = i2c_master_transmit_receive(bmp280_handle, &reg_data, 1, data_buf, 6, 1000);

        if (ret == ESP_OK) {
            int32_t raw_press = (int32_t)((((uint32_t)data_buf[0]) << 12) |
                                          (((uint32_t)data_buf[1]) << 4)  |
                                          (((uint32_t)data_buf[2]) >> 4));

            int32_t raw_temp  = (int32_t)((((uint32_t)data_buf[3]) << 12) |
                                          (((uint32_t)data_buf[4]) << 4)  |
                                          (((uint32_t)data_buf[5]) >> 4));

            int32_t t_fine;
            float temp_c = bmp280_compensate_temperature(raw_temp, &calib, &t_fine);
            float press_hpa = bmp280_compensate_pressure(raw_press, t_fine, &calib);

            ESP_LOGI(TAG, "Sicaklik: %.2f °C | Basinc: %.2f hPa", temp_c, press_hpa);
        } else {
            ESP_LOGE(TAG, "Burst okuma hatasi: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}