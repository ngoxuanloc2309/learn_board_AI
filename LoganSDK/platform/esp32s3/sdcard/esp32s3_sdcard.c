#include <stdbool.h>
#include <string.h>

#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sx_sdcard.h"

static int map_error(esp_err_t error)
{
    if (error == ESP_OK) return SX_SDCARD_OK;
    if (error == ESP_ERR_INVALID_ARG) return SX_SDCARD_ERR_PARAM;
    if (error == ESP_ERR_NO_MEM) return SX_SDCARD_ERR_NOMEM;
    return SX_SDCARD_ERR_BUS;
}

int sx_sdcard_mount(sx_sdcard_t *sdcard, const sx_sdcard_config_t *config)
{
    if (sdcard == NULL || config == NULL || sdcard->mounted ||
        config->host < 0 || config->mosi_pin < 0 || config->miso_pin < 0 ||
        config->sclk_pin < 0 || config->cs_pin < 0 || config->mount_path == NULL ||
        config->mount_path[0] == '\0' || config->max_files == 0U) {
        return sdcard != NULL && sdcard->mounted ? SX_SDCARD_ERR_STATE : SX_SDCARD_ERR_PARAM;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = (spi_host_device_t)config->host;
    if (config->frequency_khz != 0U) {
        host.max_freq_khz = config->frequency_khz;
    }
    spi_bus_config_t bus_config = {
        .mosi_io_num = config->mosi_pin,
        .miso_io_num = config->miso_pin,
        .sclk_io_num = config->sclk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000
    };
    int ret = map_error(spi_bus_initialize(host.slot, &bus_config, SPI_DMA_CH_AUTO));
    if (ret != SX_SDCARD_OK) return ret;

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = (gpio_num_t)config->cs_pin;
    slot.host_id = host.slot;
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = config->format_if_mount_failed != 0U,
        .max_files = config->max_files,
        .allocation_unit_size = 0
    };
    sdmmc_card_t *card = NULL;
    esp_err_t error = esp_vfs_fat_sdspi_mount(config->mount_path, &host, &slot,
                                               &mount_config, &card);
    if (error != ESP_OK) {
        spi_bus_free(host.slot);
        return map_error(error);
    }
    sdcard->config = config;
    sdcard->card = card;
    sdcard->mounted = 1U;
    return SX_SDCARD_OK;
}

int sx_sdcard_unmount(sx_sdcard_t *sdcard)
{
    if (sdcard == NULL) return SX_SDCARD_ERR_PARAM;
    if (!sdcard->mounted) return SX_SDCARD_OK;
    esp_err_t error = esp_vfs_fat_sdcard_unmount(sdcard->config->mount_path,
                                                  (sdmmc_card_t *)sdcard->card);
    int ret = map_error(error);
    if (ret == SX_SDCARD_OK) {
        spi_bus_free((spi_host_device_t)sdcard->config->host);
        memset(sdcard, 0, sizeof(*sdcard));
    }
    return ret;
}
