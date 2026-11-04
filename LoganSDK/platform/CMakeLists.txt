# Layer 0 - esp32s3: implements the sx_* contracts with ESP-IDF drivers.
# The only layer allowed to include ESP-IDF driver headers.
# The component name is the directory name (esp32s3).
idf_component_register(SRCS "gpio/esp32s3_gpio.c"
                            "i2c/esp32s3_i2c.c"
                            "i2s/esp32s3_i2s.c"
                            "i80/esp32s3_i80.c"
                            "spi/esp32s3_spi.c"
                    REQUIRES peripherals
                    PRIV_REQUIRES driver esp_lcd)