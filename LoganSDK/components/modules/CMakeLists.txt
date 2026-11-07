# Layer 2 - modules: logical devices (screenHMI, sdcard, speaker, touch, inmp441 microphone).
# They take configuration through parameters and never include board.h.
idf_component_register(SRCS "micro/inmp441.c"
                            "screenHMI/screenHMI.c"
                            "sdcard/sdcard.c"
                            "speaker/speaker.c"
                            "touch/touch.c"
                    INCLUDE_DIRS "micro" "screenHMI" "sdcard" "speaker" "touch"
                    REQUIRES peripherals utils)