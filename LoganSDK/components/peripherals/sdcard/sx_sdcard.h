#ifndef SX_SDCARD_H
#define SX_SDCARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define SX_SDCARD_OK           0
#define SX_SDCARD_ERR_PARAM   (-1)
#define SX_SDCARD_ERR_STATE   (-2)
#define SX_SDCARD_ERR_BUS     (-3)
#define SX_SDCARD_ERR_NOMEM   (-4)

typedef struct {
    int host;
    int mosi_pin;
    int miso_pin;
    int sclk_pin;
    int cs_pin;
    uint32_t frequency_khz;
    const char *mount_path;
    uint8_t max_files;
    uint8_t format_if_mount_failed;
} sx_sdcard_config_t;

typedef struct {
    const sx_sdcard_config_t *config;
    void *card;
    uint8_t mounted;
} sx_sdcard_t;

int sx_sdcard_mount(sx_sdcard_t *sdcard, const sx_sdcard_config_t *config);
int sx_sdcard_unmount(sx_sdcard_t *sdcard);

#ifdef __cplusplus
}
#endif

#endif
