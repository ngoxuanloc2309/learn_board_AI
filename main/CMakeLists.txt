# main: only app_main(). No logic here, it hands over to the app layer.
idf_component_register(SRCS "board_hmi_ai.c"
                    INCLUDE_DIRS "."
                    REQUIRES app)