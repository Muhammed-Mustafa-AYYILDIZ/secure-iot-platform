#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#define BMP280_ADDR 0x76

typedef struct {
    float temperature;
    float pressure;
} bmp280_data_t;

esp_err_t bmp280_init(i2c_master_dev_handle_t dev);
esp_err_t bmp280_read_data(i2c_master_dev_handle_t dev, bmp280_data_t *out_data);
