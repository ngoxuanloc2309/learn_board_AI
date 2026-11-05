# Layer 2 - modules: logical devices (screenHMI, sdcard, speaker, storage, touch).
# They take configuration through parameters and never include board.h.
idf_component_register(SRCS "screenHMI/screenHMI.c"
                            "sdcard/sdcard.c"
                            "speaker/speaker.c"
                            "storage/storage.c"
                            "touch/touch.c"
                    INCLUDE_DIRS "screenHMI" "sdcard" "speaker" "storage" "touch"
                    REQUIRES peripherals utils)