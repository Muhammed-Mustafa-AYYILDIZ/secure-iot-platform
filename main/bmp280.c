#include "bmp280.h"
#include "esp_log.h"

static const char *TAG = "BMP280";
static bmp280_calib_t s_calib;

static float bmp280_compensate_temp(int32_t raw_temp, int32_t *t_fine)
{
    int32_t var1 = ((((raw_temp >> 3) - ((int32_t)s_calib.dig_T1 << 1))) * ((int32_t)s_calib.dig_T2)) >> 11;
    int32_t var2 = (((((raw_temp >> 4) - ((int32_t)s_calib.dig_T1)) * ((raw_temp >> 4) - ((int32_t)s_calib.dig_T1))) >> 12) * ((int32_t)s_calib.dig_T3)) >> 14;

    *t_fine = var1 + var2;
    float temp = (*t_fine * 5 + 128) >> 8;
    return temp / 100.0f;
}

static float bmp280_compensate_press(int32_t raw_press, int32_t t_fine)
{
    int64_t var1 = ((int64_t)t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t)s_calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)s_calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)s_calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)s_calib.dig_P3) >> 8) + ((var1 * (int64_t)s_calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)s_calib.dig_P1) >> 33;

    if (var1 == 0) {
        return 0;
    }

    int64_t p = 1048576 - raw_press;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)s_calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)s_calib.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)s_calib.dig_P7) << 4);

    return (float)p / 256.0f / 100.0f;
}

esp_err_t bmp280_init(i2c_master_dev_handle_t dev)
{
    uint8_t reg_id = BMP280_REG_CHIP_ID;
    uint8_t chip_id = 0;

    esp_err_t ret = i2c_master_transmit_receive(dev, &reg_id, 1, &chip_id, 1, 1000);
    if (ret != ESP_OK || chip_id != 0x58) {
        ESP_LOGE(TAG, "BMP280 bulunamadi! Chip ID: 0x%02X", chip_id);
        return ESP_FAIL;
    }

    uint8_t reg_calib = BMP280_REG_CALIB;
    uint8_t calib_buf[24] = {0};
    ret = i2c_master_transmit_receive(dev, &reg_calib, 1, calib_buf, 24, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Kalibrasyon okunamadi!");
        return ret;
    }

    s_calib.dig_T1 = (uint16_t)((calib_buf[1] << 8) | calib_buf[0]);
    s_calib.dig_T2 = (int16_t)((calib_buf[3] << 8) | calib_buf[2]);
    s_calib.dig_T3 = (int16_t)((calib_buf[5] << 8) | calib_buf[4]);
    s_calib.dig_P1 = (uint16_t)((calib_buf[7] << 8) | calib_buf[6]);
    s_calib.dig_P2 = (int16_t)((calib_buf[9] << 8) | calib_buf[8]);
    s_calib.dig_P3 = (int16_t)((calib_buf[11] << 8) | calib_buf[10]);
    s_calib.dig_P4 = (int16_t)((calib_buf[13] << 8) | calib_buf[12]);
    s_calib.dig_P5 = (int16_t)((calib_buf[15] << 8) | calib_buf[14]);
    s_calib.dig_P6 = (int16_t)((calib_buf[17] << 8) | calib_buf[16]);
    s_calib.dig_P7 = (int16_t)((calib_buf[19] << 8) | calib_buf[18]);
    s_calib.dig_P8 = (int16_t)((calib_buf[21] << 8) | calib_buf[20]);
    s_calib.dig_P9 = (int16_t)((calib_buf[23] << 8) | calib_buf[22]);

    uint8_t ctrl_cmd[2] = {BMP280_REG_CTRL_MEAS, 0x27};
    return i2c_master_transmit(dev, ctrl_cmd, 2, 1000);
}

esp_err_t bmp280_read_data(i2c_master_dev_handle_t dev, bmp280_data_t *out_data)
{
    uint8_t reg_data = BMP280_REG_DATA;
    uint8_t data_buf[6] = {0};

    esp_err_t ret = i2c_master_transmit_receive(dev, &reg_data, 1, data_buf, 6, 1000);
    if (ret != ESP_OK) {
        return ret;
    }

    int32_t raw_press = (int32_t)((((uint32_t)data_buf[0]) << 12) |
                                  (((uint32_t)data_buf[1]) << 4)  |
                                  (((uint32_t)data_buf[2]) >> 4));

    int32_t raw_temp  = (int32_t)((((uint32_t)data_buf[3]) << 12) |
                                  (((uint32_t)data_buf[4]) << 4)  |
                                  (((uint32_t)data_buf[5]) >> 4));

    int32_t t_fine;
    out_data->temperature = bmp280_compensate_temp(raw_temp, &t_fine);
    out_data->pressure    = bmp280_compensate_press(raw_press, t_fine);

    return ESP_OK;
}