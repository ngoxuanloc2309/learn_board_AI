# Third party sources. The component name is the directory name (libs).
# Only FatFs (ChaN R0.16) is built here. It is used by services/storage through disk_* in storage_diskio.c.
# The ESP-IDF fatfs component is not used, so do not add it to any REQUIRES.
# cJSON is in this folder but is not built yet.
idf_component_register(SRCS "fatfs/ff.c"
                            "fatfs/ffsystem.c"
                            "fatfs/ffunicode.c"
                    INCLUDE_DIRS "fatfs")