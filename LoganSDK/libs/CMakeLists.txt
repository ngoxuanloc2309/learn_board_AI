# Third party sources. The component name is the directory name (libs).
# FatFs (ChaN R0.16) and cJSON are built here. It is used by services/storage through disk_* in storage_diskio.c.
# The ESP-IDF fatfs component is not used, so do not add it to any REQUIRES.
# cJSON is used by services/AI (PRIV_REQUIRES libs). cJSON_Utils.c is not built.
idf_component_register(SRCS "fatfs/ff.c"
                            "fatfs/ffsystem.c"
                            "fatfs/ffunicode.c"
                            "cJSON/cJSON.c"
                    INCLUDE_DIRS "fatfs" "cJSON")