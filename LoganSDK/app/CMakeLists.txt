# Layer 4 - app: tasks and start-up. The only layer allowed to see every other layer.
# esp32s3 (LoganSDK/platform/esp32s3) is listed so that the chip implementation is linked into the firmware.
# The component name is the directory name. To use another chip, replace this name.
idf_component_register(SRCS "app.c"
                            "user/AI_app/AI_app.c"
                            "user/http_app/http_app.c"
                            "user/ntp_app/ntp_app.c"
                            "user/screen_app/screen_app.c"
                            "user/speaker_app/speaker_app.c"
                    INCLUDE_DIRS "." "user/AI_app" "user/http_app" "user/ntp_app"
                                 "user/screen_app" "user/speaker_app"
                    REQUIRES board modules services utils esp32s3)