# Layer 2 - modules: logical devices (screenHMI, sdcard, speaker, touch, inmp441 microphone) and textdraw (UTF-8 text with a bitmap font).
# They take configuration through parameters and never include board.h.
idf_component_register(SRCS "micro/inmp441.c"
                            "screenHMI/screenHMI.c"
                            "sdcard/sdcard.c"
                            "speaker/speaker.c"
                            "textdraw/font_vi_8x16.c"
                            "textdraw/textdraw.c"
                            "touch/touch.c"
                    INCLUDE_DIRS "micro" "screenHMI" "sdcard" "speaker" "textdraw" "touch"
                    REQUIRES peripherals utils)