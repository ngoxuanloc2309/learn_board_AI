#ifndef SX_I80_H
#define SX_I80_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Return codes: 0 on success, negative on failure. */
#define SX_I80_OK            0
#define SX_I80_ERR_PARAM    (-1)
#define SX_I80_ERR_BUS      (-2)
#define SX_I80_ERR_NOMEM    (-3)

/* Widest bus supported by the contract. */
#define SX_I80_MAX_BUS_WIDTH 16U

/*
 * Called when a sx_i80_write_color() transfer has finished. It may run in interrupt context,
 * so it must not block. Return true when a higher priority task was woken (interrupt semantics),
 * otherwise false.
 */
typedef bool (*sx_i80_done_cb_t)(void *user_ctx);

typedef struct sx_i80_config {
    int               dc_pin;                              /* data or command select (RS) */
    int               wr_pin;                              /* write strobe */
    int               cs_pin;                              /* chip select, -1 when the panel CS is tied low */
    int               data_pins[SX_I80_MAX_BUS_WIDTH];     /* data lines, index 0 is D0 */
    uint8_t           bus_width;                           /* 8 or 16 */
    uint32_t          pclk_hz;                             /* write clock, limited by the panel datasheet */
    uint32_t          max_transfer_bytes;                  /* largest single color transfer */
    uint8_t           queue_depth;                         /* pending color transfers, at least 1 */
    bool              swap_color_bytes;                    /* swap the two bytes of every 16-bit pixel */
    sx_i80_done_cb_t  on_color_done;                       /* optional */
    void             *user_ctx;                            /* passed to on_color_done */
} sx_i80_config_t;

typedef struct sx_i80 {
    const sx_i80_config_t *config;
    void                  *bus;      /* private to the platform, do not touch */
    void                  *io;       /* private to the platform, do not touch */
    bool                   ready;
} sx_i80_t;

/*
 * Creates the I80 bus and one panel interface on it. The command and parameter width is 8 bits.
 * The config must stay valid and unchanged while i80 is in use.
 */
int sx_i80_init(sx_i80_t *i80, const sx_i80_config_t *config);

int sx_i80_deinit(sx_i80_t *i80);

/* Sends one command byte followed by len parameter bytes (param may be NULL when len is 0). Blocking. */
int sx_i80_write_cmd(sx_i80_t *i80, uint8_t cmd, const void *param, size_t len);

/*
 * Sends one command byte followed by len bytes of pixel data. len must not exceed max_transfer_bytes.
 * The transfer runs in the background: data must stay valid and unchanged until on_color_done is called.
 * Buffers should be DMA capable and aligned as the platform requires.
 */
int sx_i80_write_color(sx_i80_t *i80, uint8_t cmd, const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* SX_I80_H */