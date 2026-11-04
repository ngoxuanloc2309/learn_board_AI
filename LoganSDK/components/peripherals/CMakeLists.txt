# Layer 1 - peripherals: sx_* contracts. Headers only, no source files and no ESP-IDF dependency.
# Implementations live in platform/esp32s3.
idf_component_register(INCLUDE_DIRS "gpio" "i2c" "i2s" "i80" "spi" "pwm" "sdcard" "uart")