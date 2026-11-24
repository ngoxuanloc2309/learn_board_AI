#ifndef SX_SYSTEM_H
#define SX_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * System contract: facts about the chip that upper layers need and that must not pull in a driver header.
 * Today it holds only the factory MAC address (used to build the device_id of docs/Backend_Protocol.md).
 */

#define SX_SYSTEM_OK          0
#define SX_SYSTEM_ERR_PARAM   (-1)
#define SX_SYSTEM_ERR_BUS     (-2)

/* Length of a MAC address in bytes. */
#define SX_SYSTEM_MAC_LEN     6U

/*
 * Copies the Wi-Fi station MAC address (6 bytes, factory value from eFuse) into mac.
 * The value does not change between boots and does not need Wi-Fi to be started.
 * Returns SX_SYSTEM_ERR_PARAM when mac is NULL and SX_SYSTEM_ERR_BUS when the chip cannot give the address.
 * May be called from any task.
 */
int sx_system_get_mac(uint8_t mac[SX_SYSTEM_MAC_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* SX_SYSTEM_H */