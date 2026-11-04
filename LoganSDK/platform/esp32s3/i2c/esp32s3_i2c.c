#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sx_i2c.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "soc/soc_caps.h"

#define I2C_ADDR7_MAX        0x7FU
#define I2C_REG_SIZE_MAX     2U
#define I2C_GLITCH_FILTER    7U

typedef struct {
    uint8_t                 addr;
    i2c_master_dev_handle_t handle;
} i2c_slot_t;

/* One entry per controller. The driver needs one device handle per address, kept here. */
typedef struct {
    bool                    used;
    i2c_master_bus_handle_t bus;
    uint32_t                clock_hz;
    uint8_t                 count;
    i2c_slot_t              slot[SX_I2C_MAX_DEVICES];
    SemaphoreHandle_t       lock;
    StaticSemaphore_t       lock_storage;
} i2c_port_state_t;

static i2c_port_state_t s_port[SOC_I2C_NUM];

static int map_err(esp_err_t err)
{
    switch (err) {
    case ESP_OK:               return SX_I2C_OK;
    case ESP_ERR_INVALID_ARG:  return SX_I2C_ERR_PARAM;
    case ESP_ERR_TIMEOUT:      return SX_I2C_ERR_TIMEOUT;
    case ESP_ERR_NO_MEM:       return SX_I2C_ERR_NOMEM;
    case ESP_ERR_NOT_FOUND:    return SX_I2C_ERR_NACK;   /* only i2c_master_probe reports this */
    default:                   return SX_I2C_ERR_BUS;
    }
}

static int clamp_timeout(uint32_t timeout_ms)
{
    return (timeout_ms > (uint32_t)INT32_MAX) ? INT32_MAX : (int)timeout_ms;
}

static i2c_port_state_t *get_port(const sx_i2c_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }
    if ((config->port < 0) || (config->port >= (int)SOC_I2C_NUM)) {
        return NULL;
    }
    i2c_port_state_t *port = &s_port[config->port];
    return port->used ? port : NULL;
}

/* Returns the device handle for addr7, adding the device to the bus on first use. */
static int get_device(i2c_port_state_t *port, uint8_t addr7, i2c_master_dev_handle_t *out)
{
    int ret = SX_I2C_OK;

    xSemaphoreTake(port->lock, portMAX_DELAY);

    for (uint8_t i = 0; i < port->count; i++) {
        if (port->slot[i].addr == addr7) {
            *out = port->slot[i].handle;
            xSemaphoreGive(port->lock);
            return SX_I2C_OK;
        }
    }

    if (port->count >= SX_I2C_MAX_DEVICES) {
        ret = SX_I2C_ERR_NOMEM;
    } else {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address  = addr7,
            .scl_speed_hz    = port->clock_hz,
        };
        i2c_master_dev_handle_t handle = NULL;
        ret = map_err(i2c_master_bus_add_device(port->bus, &dev_cfg, &handle));
        if (ret == SX_I2C_OK) {
            port->slot[port->count].addr = addr7;
            port->slot[port->count].handle = handle;
            port->count++;
            *out = handle;
        }
    }

    xSemaphoreGive(port->lock);
    return ret;
}

static int encode_reg(uint8_t *buf, uint16_t reg, uint8_t reg_size)
{
    if (reg_size == 1U) {
        buf[0] = (uint8_t)(reg & 0xFFU);
        return SX_I2C_OK;
    }
    if (reg_size == 2U) {
        buf[0] = (uint8_t)(reg >> 8);
        buf[1] = (uint8_t)(reg & 0xFFU);
        return SX_I2C_OK;
    }
    return SX_I2C_ERR_PARAM;
}

int sx_i2c_init(const sx_i2c_config_t *config)
{
    if ((config == NULL) || (config->port < 0) || (config->port >= (int)SOC_I2C_NUM) ||
        (config->sda_pin < 0) || (config->scl_pin < 0) || (config->clock_hz == 0U)) {
        return SX_I2C_ERR_PARAM;
    }

    i2c_port_state_t *port = &s_port[config->port];
    if (port->used) {
        return SX_I2C_ERR_BUS;
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port          = config->port,
        .sda_io_num        = config->sda_pin,
        .scl_io_num        = config->scl_pin,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = I2C_GLITCH_FILTER,
        .flags.enable_internal_pullup = config->internal_pullup,
    };

    i2c_master_bus_handle_t bus = NULL;
    int ret = map_err(i2c_new_master_bus(&bus_cfg, &bus));
    if (ret != SX_I2C_OK) {
        return ret;
    }

    memset(port->slot, 0, sizeof(port->slot));
    port->count    = 0;
    port->bus      = bus;
    port->clock_hz = config->clock_hz;
    port->lock     = xSemaphoreCreateMutexStatic(&port->lock_storage);
    port->used     = true;
    return SX_I2C_OK;
}

int sx_i2c_deinit(const sx_i2c_config_t *config)
{
    i2c_port_state_t *port = get_port(config);
    if (port == NULL) {
        return SX_I2C_ERR_PARAM;
    }

    xSemaphoreTake(port->lock, portMAX_DELAY);

    for (uint8_t i = 0; i < port->count; i++) {
        if (port->slot[i].handle != NULL) {
            int ret = map_err(i2c_master_bus_rm_device(port->slot[i].handle));
            if (ret != SX_I2C_OK) {
                xSemaphoreGive(port->lock);
                return ret;
            }
            port->slot[i].handle = NULL;
        }
    }
    port->count = 0;

    int ret = map_err(i2c_del_master_bus(port->bus));
    if (ret != SX_I2C_OK) {
        xSemaphoreGive(port->lock);
        return ret;
    }

    port->bus  = NULL;
    port->used = false;
    xSemaphoreGive(port->lock);
    vSemaphoreDelete(port->lock);
    port->lock = NULL;
    return SX_I2C_OK;
}

int sx_i2c_write(const sx_i2c_config_t *config, uint8_t addr7, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    i2c_port_state_t *port = get_port(config);
    if ((port == NULL) || (addr7 > I2C_ADDR7_MAX) || (data == NULL) || (len == 0U)) {
        return SX_I2C_ERR_PARAM;
    }

    i2c_master_dev_handle_t dev = NULL;
    int ret = get_device(port, addr7, &dev);
    if (ret != SX_I2C_OK) {
        return ret;
    }
    return map_err(i2c_master_transmit(dev, data, len, clamp_timeout(timeout_ms)));
}

int sx_i2c_read(const sx_i2c_config_t *config, uint8_t addr7, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    i2c_port_state_t *port = get_port(config);
    if ((port == NULL) || (addr7 > I2C_ADDR7_MAX) || (data == NULL) || (len == 0U)) {
        return SX_I2C_ERR_PARAM;
    }

    i2c_master_dev_handle_t dev = NULL;
    int ret = get_device(port, addr7, &dev);
    if (ret != SX_I2C_OK) {
        return ret;
    }
    return map_err(i2c_master_receive(dev, data, len, clamp_timeout(timeout_ms)));
}

int sx_i2c_mem_write(const sx_i2c_config_t *config, uint8_t addr7, uint16_t reg, uint8_t reg_size,
                     const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    i2c_port_state_t *port = get_port(config);
    if ((port == NULL) || (addr7 > I2C_ADDR7_MAX) || ((data == NULL) && (len != 0U))) {
        return SX_I2C_ERR_PARAM;
    }

    uint8_t reg_buf[I2C_REG_SIZE_MAX];
    int ret = encode_reg(reg_buf, reg, reg_size);
    if (ret != SX_I2C_OK) {
        return ret;
    }

    i2c_master_dev_handle_t dev = NULL;
    ret = get_device(port, addr7, &dev);
    if (ret != SX_I2C_OK) {
        return ret;
    }

    /* Register bytes and payload go out in one transaction without copying the payload. */
    i2c_master_transmit_multi_buffer_info_t parts[2] = {
        { .write_buffer = reg_buf,                 .buffer_size = reg_size },
        { .write_buffer = (uint8_t *)(uintptr_t)data, .buffer_size = len },
    };
    return map_err(i2c_master_multi_buffer_transmit(dev, parts, 2, clamp_timeout(timeout_ms)));
}

int sx_i2c_mem_read(const sx_i2c_config_t *config, uint8_t addr7, uint16_t reg, uint8_t reg_size,
                    uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    i2c_port_state_t *port = get_port(config);
    if ((port == NULL) || (addr7 > I2C_ADDR7_MAX) || (data == NULL) || (len == 0U)) {
        return SX_I2C_ERR_PARAM;
    }

    uint8_t reg_buf[I2C_REG_SIZE_MAX];
    int ret = encode_reg(reg_buf, reg, reg_size);
    if (ret != SX_I2C_OK) {
        return ret;
    }

    i2c_master_dev_handle_t dev = NULL;
    ret = get_device(port, addr7, &dev);
    if (ret != SX_I2C_OK) {
        return ret;
    }
    return map_err(i2c_master_transmit_receive(dev, reg_buf, reg_size, data, len, clamp_timeout(timeout_ms)));
}

int sx_i2c_probe(const sx_i2c_config_t *config, uint8_t addr7, uint32_t timeout_ms)
{
    i2c_port_state_t *port = get_port(config);
    if ((port == NULL) || (addr7 > I2C_ADDR7_MAX)) {
        return SX_I2C_ERR_PARAM;
    }
    return map_err(i2c_master_probe(port->bus, addr7, clamp_timeout(timeout_ms)));
}

int sx_i2c_reset(const sx_i2c_config_t *config)
{
    i2c_port_state_t *port = get_port(config);
    if (port == NULL) {
        return SX_I2C_ERR_PARAM;
    }
    return map_err(i2c_master_bus_reset(port->bus));
}