idf_component_register(SRCS "main.c" "lcd1602.c" "bmp280.c" "wifi_app.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_wifi nvs_flash esp_event esp_netif esp_driver_i2c esp_driver_gpio)