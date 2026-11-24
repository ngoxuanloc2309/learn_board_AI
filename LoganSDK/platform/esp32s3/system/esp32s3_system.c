#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_mac.h"

#include "sx_system.h"

int sx_system_get_mac(uint8_t mac[SX_SYSTEM_MAC_LEN])
{
    if (mac == NULL) {
        return SX_SYSTEM_ERR_PARAM;
    }
    /* esp_read_mac fills 6 bytes for the Wi-Fi station type. It reads the eFuse, so Wi-Fi need not run. */
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        return SX_SYSTEM_ERR_BUS;
    }
    return SX_SYSTEM_OK;
}