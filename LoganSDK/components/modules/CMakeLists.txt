# Layer 2 - modules: logical devices (screenHMI, sdcard, speaker, touch).
# They take configuration through parameters and never include board.h.
idf_component_register(SRCS "screenHMI/screenHMI.c"
                            "sdcard/sdcard.c"
                            "speaker/speaker.c"
                            "touch/touch.c"
                    INCLUDE_DIRS "screenHMI" "sdcard" "speaker" "touch"
                    REQUIRES peripherals utils)