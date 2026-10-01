#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

#define I2C_MASTER_SDA_IO   8
#define I2C_MASTER_SCL_IO   9
#define LCD_ADDR            0x27

static const char *TAG = "LCD_1602";
static i2c_master_dev_handle_t lcd_handle;

// PCF8574 Pin Haritası: [D7, D6, D5, D4, Backlight, EN, RW, RS]
#define LCD_BACKLIGHT   0x08
#define LCD_ENABLE      0x04
#define LCD_RS_DATA     0x01
#define LCD_RS_CMD      0x00

static void lcd_write_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = (nibble & 0xF0) | LCD_BACKLIGHT | rs;
    
    // EN high pulse
    uint8_t buf[1] = { data | LCD_ENABLE };
    i2c_master_transmit(lcd_handle, buf, 1, 50);
    esp_rom_delay_us(50);

    // EN low pulse
    buf[0] = data & ~LCD_ENABLE;
    i2c_master_transmit(lcd_handle, buf, 1, 50);
    esp_rom_delay_us(50);
}

static void lcd_send(uint8_t value, uint8_t rs)
{
    lcd_write_nibble(value & 0xF0, rs);        // High nibble
    lcd_write_nibble((value << 4) & 0xF0, rs); // Low nibble
}

static void lcd_cmd(uint8_t cmd)
{
    lcd_send(cmd, LCD_RS_CMD);
    vTaskDelay(pdMS_TO_TICKS(5));
}

static void lcd_data(uint8_t data)
{
    lcd_send(data, LCD_RS_DATA);
    esp_rom_delay_us(50);
}

static void lcd_print(const char *str)
{
    while (*str) {
        lcd_data((uint8_t)*str);
        str++;
    }
}

static void lcd_set_cursor(uint8_t col, uint8_t row)
{
    uint8_t row_offsets[] = {0x00, 0x40};
    lcd_cmd(0x80 | (col + row_offsets[row]));
}

static void lcd_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(50)); // Açılış beklemesi

    // 4-bit moda geçiş sırası
    lcd_write_nibble(0x30, LCD_RS_CMD);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(0x30, LCD_RS_CMD);
    esp_rom_delay_us(150);
    lcd_write_nibble(0x30, LCD_RS_CMD);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(0x20, LCD_RS_CMD); // 4-bit arayüz
    vTaskDelay(pdMS_TO_TICKS(5));

    // Fonksiyon ayarları
    lcd_cmd(0x28); // 4-bit, 2 satır, 5x8 font
    lcd_cmd(0x0C); // Ekran açık, imleç kapalı (Display ON, Cursor OFF)
    lcd_cmd(0x06); // Giriş modu: Otomatik sağa kaydır
    lcd_cmd(0x01); // Ekranı temizle
    vTaskDelay(pdMS_TO_TICKS(5));
}

void app_main(void)
{
    ESP_LOGI(TAG, "I2C ve LCD 1602 Baslatiliyor...");

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = LCD_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &lcd_handle));

    lcd_init();

    // 1. Satıra yaz
    lcd_set_cursor(2, 0);
    lcd_print("Burak'in YARRAĞA!");


    

    ESP_LOGI(TAG, "LCD ekran basariyla yazildi!");
}