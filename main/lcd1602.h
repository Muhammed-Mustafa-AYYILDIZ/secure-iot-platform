#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#define LCD_ADDR 0x27

esp_err_t lcd_init(i2c_master_dev_handle_t dev);
void lcd_send_cmd(i2c_master_dev_handle_t dev, uint8_t cmd);
void lcd_send_char(i2c_master_dev_handle_t dev, char ch);
void lcd_send_string(i2c_master_dev_handle_t dev, const char *str);
void lcd_set_cursor(i2c_master_dev_handle_t dev, uint8_t col, uint8_t row);
void lcd_clear(i2c_master_dev_handle_t dev);