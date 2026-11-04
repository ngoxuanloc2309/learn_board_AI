# Layer 4 - board: pins, port numbers and wiring (board_init).
# Uses only the sx_* contracts (peripherals) and modules. Never includes esp32s3.
idf_component_register(SRCS "board.c"
                    INCLUDE_DIRS "."
                    REQUIRES peripherals modules)