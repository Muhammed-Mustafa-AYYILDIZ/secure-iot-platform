idf_component_register(SRCS "bmp280.c" "lcd1602.c" "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_i2c)
