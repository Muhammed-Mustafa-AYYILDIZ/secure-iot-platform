#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#define I2C_SDA_PIN         GPIO_NUM_8
#define I2C_SCL_PIN         GPIO_NUM_9

// I2C Adresleri
#define LCD_ADDR            0x27
#define BMP280_ADDR         0x76

// PCF8574 Pin Haritası (LCD Backpack)
#define LCD_RS              (1 << 0)
#define LCD_RW              (1 << 1)
#define LCD_EN              (1 << 2)
#define LCD_BACKLIGHT       (1 << 3)

// BMP280 Register'ları
#define BMP280_REG_CALIB    0x88
#define BMP280_REG_CTRL_MEAS 0xF4
#define BMP280_REG_DATA     0xF7

static const char *TAG = "APP_MAIN";

// --- BMP280 Kalibrasyon Yapısı ---
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

// --- LCD1602 Düşük Seviye Fonksiyonları ---
static esp_err_t lcd_write_byte(i2c_master_dev_handle_t dev, uint8_t val)
{
    return i2c_master_transmit(dev, &val, 1, 1000);
}

static void lcd_pulse_enable(i2c_master_dev_handle_t dev, uint8_t nibble)
{
    lcd_write_byte(dev, nibble | LCD_EN | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_write_byte(dev, (nibble & ~LCD_EN) | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));
}

static void lcd_send_4bits(i2c_master_dev_handle_t dev, uint8_t data, uint8_t mode)
{
    uint8_t upper = (data & 0xF0) | mode | LCD_BACKLIGHT;
    uint8_t lower = ((data << 4) & 0xF0) | mode | LCD_BACKLIGHT;
    lcd_pulse_enable(dev, upper);
    lcd_pulse_enable(dev, lower);
}

static void lcd_send_cmd(i2c_master_dev_handle_t dev, uint8_t cmd)
{
    lcd_send_4bits(dev, cmd, 0);
}

static void lcd_send_char(i2c_master_dev_handle_t dev, char ch)
{
    lcd_send_4bits(dev, (uint8_t)ch, LCD_RS);
}

static void lcd_send_string(i2c_master_dev_handle_t dev, const char *str)
{
    while (*str) {
        lcd_send_char(dev, *str++);
    }
}

static void lcd_init(i2c_master_dev_handle_t dev)
{
    vTaskDelay(pdMS_TO_TICKS(50));
    // HD44780 4-bit başlatma sekansı
    lcd_pulse_enable(dev, 0x30 | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_pulse_enable(dev, 0x30 | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_pulse_enable(dev, 0x30 | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_pulse_enable(dev, 0x20 | LCD_BACKLIGHT); // 4-bit moda geçiş
    vTaskDelay(pdMS_TO_TICKS(1));

    lcd_send_cmd(dev, 0x28); // 4-bit, 2 satır, 5x8 font
    lcd_send_cmd(dev, 0x08); // Ekran kapalı
    lcd_send_cmd(dev, 0x01); // Ekranı temizle
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_send_cmd(dev, 0x06); // İmleç sağa kaysın
    lcd_send_cmd(dev, 0x0C); // Ekran açık, imleç kapalı
}

static void lcd_set_cursor(i2c_master_dev_handle_t dev, uint8_t col, uint8_t row)
{
    uint8_t row_offsets[] = {0x00, 0x40};
    lcd_send_cmd(dev, 0x80 | (col + row_offsets[row]));
}

// --- BMP280 Kompanzasyon Fonksiyonları ---
static float bmp280_compensate_temperature(int32_t raw_temp, bmp280_calib_t *calib, int32_t *t_fine)
{
    int32_t var1 = ((((raw_temp >> 3) - ((int32_t)calib->dig_T1 << 1))) * ((int32_t)calib->dig_T2)) >> 11;
    int32_t var2 = (((((raw_temp >> 4) - ((int32_t)calib->dig_T1)) * ((raw_temp >> 4) - ((int32_t)calib->dig_T1))) >> 12) * ((int32_t)calib->dig_T3)) >> 14;
    *t_fine = var1 + var2;
    float temp = (*t_fine * 5 + 128) >> 8;
    return temp / 100.0f;
}

static float bmp280_compensate_pressure(int32_t raw_press, int32_t t_fine, bmp280_calib_t *calib)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib->dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib->dig_P5) << 17);
    var2 = var2 + (((int64_t)calib->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib->dig_P3) >> 8) + ((var1 * (int64_t)calib->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib->dig_P1) >> 33;
    if (var1 == 0) return 0.0f;
    p = 1048576 - raw_press;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)calib->dig_P7) << 4);
    return (float)p / 25600.0f;
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

    // 1. LCD Aygıtı
    i2c_device_config_t lcd_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = LCD_ADDR,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t lcd_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &lcd_cfg, &lcd_handle));

    // 2. BMP280 Aygıtı
    i2c_device_config_t bmp_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BMP280_ADDR,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t bmp280_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &bmp_cfg, &bmp280_handle));

    // LCD Başlat
    lcd_init(lcd_handle);
    lcd_set_cursor(lcd_handle, 0, 0);
    lcd_send_string(lcd_handle, "Sistem Basliyor");
    vTaskDelay(pdMS_TO_TICKS(1000));
    lcd_send_cmd(lcd_handle, 0x01); // Temizle

    // BMP280 Kalibrasyon Oku
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

    // BMP280 Normal Mod Başlat
    uint8_t ctrl_cmd[2] = {BMP280_REG_CTRL_MEAS, 0x27};
    ESP_ERROR_CHECK(i2c_master_transmit(bmp280_handle, ctrl_cmd, 2, 1000));
    ESP_LOGI(TAG, "Sistem hazir, veriler ekrana yaziliyor...");

    char line_buf[17];

    while (1) {
        uint8_t reg_data = BMP280_REG_DATA;
        uint8_t data_buf[6] = {0};

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

            // Terminal Logu
            ESP_LOGI(TAG, "T: %.2f C | P: %.1f hPa", temp_c, press_hpa);

            // LCD 1. Satır: Sıcaklık
            snprintf(line_buf, sizeof(line_buf), "Temp:  %.2f C", temp_c);
            lcd_set_cursor(lcd_handle, 0, 0);
            lcd_send_string(lcd_handle, line_buf);

            // LCD 2. Satır: Basınç
            snprintf(line_buf, sizeof(line_buf), "Press: %.1f hPa", press_hpa);
            lcd_set_cursor(lcd_handle, 0, 1);
            lcd_send_string(lcd_handle, line_buf);
        } else {
            ESP_LOGE(TAG, "Sensor okunamadi!");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}