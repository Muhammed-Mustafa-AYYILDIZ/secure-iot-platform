#include "lcd1602.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LCD_RS         (1 << 0)
#define LCD_RW         (1 << 1)
#define LCD_EN         (1 << 2)
#define LCD_BACKLIGHT  (1 << 3)

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

esp_err_t lcd_init(i2c_master_dev_handle_t dev)
{
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_pulse_enable(dev, 0x30 | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_pulse_enable(dev, 0x30 | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_pulse_enable(dev, 0x30 | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_pulse_enable(dev, 0x20 | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));

    lcd_send_cmd(dev, 0x28);
    lcd_send_cmd(dev, 0x08);
    lcd_clear(dev);
    lcd_send_cmd(dev, 0x06);
    lcd_send_cmd(dev, 0x0C);
    return ESP_OK;
}

void lcd_clear(i2c_master_dev_handle_t dev)
{
    lcd_send_cmd(dev, 0x01);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd_set_cursor(i2c_master_dev_handle_t dev, uint8_t col, uint8_t row)
{
    uint8_t row_offsets[] = {0x00, 0x40};
    lcd_send_cmd(dev, 0x80 | (col + row_offsets[row]));
}

void lcd_send_string(i2c_master_dev_handle_t dev, const char *str)
{
    while (*str) {
        lcd_send_char(dev, *str++);
    }
}
