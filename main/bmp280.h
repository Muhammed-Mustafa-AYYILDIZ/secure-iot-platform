#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#define BMP280_ADDR             0x76

#define BMP280_REG_CALIB        0x88
#define BMP280_REG_CHIP_ID      0xD0
#define BMP280_REG_CTRL_MEAS    0xF4
#define BMP280_REG_CONFIG       0xF5
#define BMP280_REG_DATA         0xF7

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

typedef struct {
    float temperature;
    float pressure;
} bmp280_data_t;

esp_err_t bmp280_init(i2c_master_dev_handle_t dev);
esp_err_t bmp280_read_data(i2c_master_dev_handle_t dev, bmp280_data_t *out_data);