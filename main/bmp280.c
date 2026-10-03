#include "bmp280.h"
#include "esp_log.h"
// ... (Diğer include ve define tanımları)

static bmp280_calib_t s_calib; // Sensörün fabrikasyon hatalarını düzeltecek katsayılar bellekte tutulur.

static float bmp280_compensate_temp(int32_t raw_temp, int32_t *t_fine)
{
    // Bosch'un datasheet formülü. Bit kaydırmalar (>> 11) bölme işlemi görevi görür.
    // Float (ondalıklı) kullanmak yerine tamsayılarla çok hızlı hesaplama yapılır.
    int32_t var1 = ((((raw_temp >> 3) - ((int32_t)s_calib.dig_T1 << 1))) * ((int32_t)s_calib.dig_T2)) >> 11;
    int32_t var2 = (((((raw_temp >> 4) - ((int32_t)s_calib.dig_T1)) * ((raw_temp >> 4) - ((int32_t)s_calib.dig_T1))) >> 12) * ((int32_t)s_calib.dig_T3)) >> 14;
    
    *t_fine = var1 + var2; // Bu değer basınç hesaplanırken genleşme payını düşmek için kullanılır.
    float temp = (*t_fine * 5 + 128) >> 8;
    return temp / 100.0f; // Nihayetinde insanın okuyabileceği Celsius formatına çevrilir.
}

esp_err_t bmp280_init(i2c_master_dev_handle_t dev)
{
    uint8_t reg_id = BMP280_REG_CHIP_ID;
    uint8_t chip_id = 0;
    // 0xD0 adresli yazmacı oku. BMP280 ise her zaman 0x58 cevabını vermelidir. Kimlik doğrulaması.
    esp_err_t ret = i2c_master_transmit_receive(dev, &reg_id, 1, &chip_id, 1, 1000);
    
    // ... (Hata kontrolü)

    uint8_t reg_calib = BMP280_REG_CALIB;
    uint8_t calib_buf[24] = {0};
    // 0x88 adresinden başlayarak 24 baytlık fabrika kalibrasyon verisini tek seferde çek.
    ret = i2c_master_transmit_receive(dev, &reg_calib, 1, calib_buf, 24, 1000);

    // I2C 8 bit veri getirir. Biz bunları Little-Endian kuralına göre 16 bitlik (2 bayt) katsayılara dönüştürüyorum.
    s_calib.dig_T1 = (uint16_t)((calib_buf[1]  << 8) | calib_buf[0]); // Üst baytı 8 bit sola kaydır, alt bayt ile birleştir.
    // ... (Diğer katsayılar)

    // 0xF4 yazmacına 0x27 göndererek sensöre "Normal modda ölçüme başla" talimatı verilir.
    uint8_t ctrl_cmd[2] = {BMP280_REG_CTRL_MEAS, 0x27};
    return i2c_master_transmit(dev, ctrl_cmd, 2, 1000);
}

esp_err_t bmp280_read_data(i2c_master_dev_handle_t dev, bmp280_data_t *out_data)
{
    uint8_t reg_data = BMP280_REG_DATA;
    uint8_t data_buf[6] = {0};

    // 0xF7 adresinden başlayarak 6 bayt oku: 3 bayt basınç, 3 bayt sıcaklık ham verisi.
    esp_err_t ret = i2c_master_transmit_receive(dev, &reg_data, 1, data_buf, 6, 1000);
    
    // 3 adet 8-bitlik parçayı, 20-bitlik tek bir tam sayı haline getirme işlemi.
    int32_t raw_press = (int32_t)((((uint32_t)data_buf[0]) << 12) |
                                  (((uint32_t)data_buf[1]) << 4)  |
                                  (((uint32_t)data_buf[2]) >> 4)); // Alt baytın gereksiz 4 bitini çöpe at (>> 4).
    
    // ... (Sıcaklık ham verisinin birleştirilmesi)

    int32_t t_fine;
    out_data->temperature = bmp280_compensate_temp(raw_temp, &t_fine);
    out_data->pressure    = bmp280_compensate_press(raw_press, t_fine);
    return ESP_OK;
}