# Layer 4 - board: pins, port numbers and wiring (board_init).
# Uses only the sx_* contracts (peripherals), modules and the storage service. Never includes esp32s3.
# services is needed because board.c owns the storage configuration (storage_config_t).
idf_component_register(SRCS "board.c"
                    INCLUDE_DIRS "."
                    REQUIRES peripherals modules services)