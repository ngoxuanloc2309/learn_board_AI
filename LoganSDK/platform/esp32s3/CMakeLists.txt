# Layer 0 - ESP32-S3 implementations of the peripheral contracts.
idf_component_register(SRCS "gpio/esp32s3_gpio.c"
                            "i2c/esp32s3_i2c.c"
                            "i2s/esp32s3_i2s.c"
                            "i80/esp32s3_i80.c"
                            "pwm/esp32s3_pwm.c"
                            "spi/esp32s3_spi.c"
                            "time/esp32s3_time.c"
                            "uart/esp32s3_uart.c"
                    INCLUDE_DIRS "gpio" "i2c" "i2s" "i80" "pwm" "spi" "uart"
                    REQUIRES peripherals
                    PRIV_REQUIRES driver esp_lcd esp_timer esp_rom freertos)