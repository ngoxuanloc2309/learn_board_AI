# Layer 2 - modules: logical devices (screenHMI, speaker, storage, touch).
# They take configuration through parameters and never include board.h.
idf_component_register(SRCS "screenHMI/screenHMI.c"
                            "speaker/speaker.c"
                            "storage/storage.c"
                            "touch/touch.c"
                    INCLUDE_DIRS "screenHMI" "speaker" "storage" "touch"
                    REQUIRES peripherals utils)