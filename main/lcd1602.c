#include "lcd1602.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// PCF8574 modülünün bacak bağlantıları. İlgili bit 1 yapıldığında o bacağa elektrik (3.3V/5V) gider.
#define LCD_RS         (1 << 0) // Register Select: 0 ise Komut (ekranı temizle), 1 ise Veri ('A' harfi)
#define LCD_RW         (1 << 1) // Read/Write: Hep 0'da tutulur (sadece yazma yapıyoruz)
#define LCD_EN         (1 << 2) // Enable pini: Ekranın veriyi okuması için tetikleyici
#define LCD_BACKLIGHT  (1 << 3) // Arka ışığı yakar

static void lcd_pulse_enable(i2c_master_dev_handle_t dev, uint8_t nibble)
{
    // EN pinini HIGH (1) yap, bekle, LOW (0) yap. Bu elektriksel sinyal dalgası 
    // LCD'ye "Hattaki veri hazır, şimdi işlemciye al" demektir.
    lcd_write_byte(dev, nibble | LCD_EN | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_write_byte(dev, (nibble & ~LCD_EN) | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));
}

static void lcd_send_4bits(i2c_master_dev_handle_t dev, uint8_t data, uint8_t mode)
{
    // PCF8574'ün sadece 4 veri pini bağlı. 8 bitlik veriyi ikiye bölüp 4 bit / 4 bit gönderiyorum.
    uint8_t upper = (data & 0xF0) | mode | LCD_BACKLIGHT;         // Verinin ilk 4 biti
    uint8_t lower = ((data << 4) & 0xF0) | mode | LCD_BACKLIGHT;  // Verinin son 4 biti
    lcd_pulse_enable(dev, upper);
    lcd_pulse_enable(dev, lower);
}

esp_err_t lcd_init(i2c_master_dev_handle_t dev)
{
    vTaskDelay(pdMS_TO_TICKS(50)); // Ekranın elektriksel olarak kendine gelmesi için bekleme
    // Datasheet'te belirtilen özel başlangıç (Initialization) sinyalleri. 
    // Ekranı 4-bit modunda çalışmaya zorluyorum.
    lcd_pulse_enable(dev, 0x30 | LCD_BACKLIGHT);
    // ... (Bekleme ve diğer 0x30 komutları)
    lcd_pulse_enable(dev, 0x20 | LCD_BACKLIGHT); // 4-bit moda kesin geçiş
    
    lcd_send_cmd(dev, 0x28); // 4-bit mod, 2 satır ekran, 5x8 piksel font
    lcd_send_cmd(dev, 0x08); // Ekranı kapat
    lcd_clear(dev);          // İçeriği temizle
    lcd_send_cmd(dev, 0x06); // Yazdıkça imleci sağa kaydır
    lcd_send_cmd(dev, 0x0C); // Ekranı aç, imleci gizle
    return ESP_OK;
}