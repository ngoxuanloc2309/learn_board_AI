#ifndef SX_I2C_H
#define SX_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Return codes: 0 on success, negative on failure. */
#define SX_I2C_OK            0
#define SX_I2C_ERR_PARAM    (-1)
#define SX_I2C_ERR_BUS      (-2)   /* bus error, arbitration lost, busy, anything else */
#define SX_I2C_ERR_NACK     (-3)   /* address or data not acknowledged (see note at sx_i2c_probe) */
#define SX_I2C_ERR_TIMEOUT  (-4)
#define SX_I2C_ERR_NOMEM    (-5)   /* no free device slot or driver out of memory */

/* Maximum number of different device addresses used on one bus. */
#define SX_I2C_MAX_DEVICES   8U

typedef struct sx_i2c_config {
    int      port;              /* controller number of the chip, taken from the board layer */
    int      sda_pin;
    int      scl_pin;
    uint32_t clock_hz;          /* bus clock, for example 100000 or 400000 */
    bool     internal_pullup;   /* enable the chip pull-ups (use only if the board has no external ones) */
} sx_i2c_config_t;

/*
 * Creates the bus described by config. The config must stay valid and unchanged while the bus is in use.
 * Calling init twice for the same port without deinit returns SX_I2C_ERR_BUS.
 */
int sx_i2c_init(const sx_i2c_config_t *config);

/* Releases every device and the bus of config->port. */
int sx_i2c_deinit(const sx_i2c_config_t *config);

/* All addresses are 7-bit (0x00 to 0x7F), NOT pre-shifted. All calls block up to timeout_ms. */
int sx_i2c_write(const sx_i2c_config_t *config, uint8_t addr7, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
int sx_i2c_read(const sx_i2c_config_t *config, uint8_t addr7, uint8_t *data, uint16_t len, uint32_t timeout_ms);

/* Register access: reg_size is 1 or 2 bytes (a 16-bit register is sent high byte first). */
int sx_i2c_mem_write(const sx_i2c_config_t *config, uint8_t addr7, uint16_t reg, uint8_t reg_size,
                     const uint8_t *data, uint16_t len, uint32_t timeout_ms);
int sx_i2c_mem_read(const sx_i2c_config_t *config, uint8_t addr7, uint16_t reg, uint8_t reg_size,
                    uint8_t *data, uint16_t len, uint32_t timeout_ms);

/*
 * Returns SX_I2C_OK when the device acknowledges its address, SX_I2C_ERR_NACK when nobody answers.
 * Note for the ESP32-S3 platform: the chip driver reports a NACK during write or read as a generic
 * state error, so those calls return SX_I2C_ERR_BUS. Use this function to tell a missing device
 * from a bus problem.
 */
int sx_i2c_probe(const sx_i2c_config_t *config, uint8_t addr7, uint32_t timeout_ms);

/* Recovers a stuck bus (for example a slave holding SDA low). Devices stay registered. */
int sx_i2c_reset(const sx_i2c_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* SX_I2C_H */