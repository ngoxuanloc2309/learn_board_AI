#ifndef TEXTDRAW_H
#define TEXTDRAW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "font_vi_8x16.h"

/*
 * Draws UTF-8 text with the 8x16 bitmap font of font_vi_8x16 (ASCII and Vietnamese letters, NFC form).
 * It knows nothing about the display: it renders one glyph cell at a time into its own buffer and
 * hands the cell to the blit function of the caller (for example a wrapper of screen_hmi_draw_bitmap).
 * Pixels are RGB565 values in native uint16_t, as screen_hmi_draw_bitmap expects them.
 *
 * The blit function must return only after the transfer finished, because the cell buffer is reused
 * for the next glyph. It returns 0 on success and a negative value on failure.
 *
 * Text must be NFC (a letter with its marks is one code point). A code point that is not in the font,
 * a decomposed mark, or bad UTF-8 is drawn as a hollow box. A new line character moves to the next
 * row, a carriage return is ignored, text wraps at the edge of the box, and drawing stops when the
 * box is full. The module never allocates memory.
 *
 * Not thread safe. The caller must serialise all calls on one textdraw_t.
 */

#define TEXTDRAW_OK              0
#define TEXTDRAW_TRUNCATED       1      /* success, but the text did not fit in the box */
#define TEXTDRAW_ERR_PARAM      (-1)    /* NULL pointer, bad configuration, or a box outside the screen */
#define TEXTDRAW_ERR_STATE      (-2)    /* not initialised */
#define TEXTDRAW_ERR_BLIT       (-3)    /* the blit function failed */

/* Largest integer scale. Sets the size of the cell buffer. Scale 2 gives 16x32 pixel cells. */
#define TEXTDRAW_MAX_SCALE       2U

/* Pixels of the largest cell. */
#define TEXTDRAW_CELL_PIXELS    (FONT_VI_GLYPH_W * FONT_VI_GLYPH_H * TEXTDRAW_MAX_SCALE * TEXTDRAW_MAX_SCALE)

/*
 * Draws the rectangle x0..x1, y0..y1 (end coordinates inclusive) from pixels. len is the size in
 * bytes. Returns only when the transfer is done. Returns 0 on success.
 */
typedef int (*textdraw_blit_fn_t)(void *ctx, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                                  const uint16_t *pixels, size_t len);

typedef struct textdraw_config {
    textdraw_blit_fn_t blit;      /* required */
    void              *ctx;       /* passed to blit */
    uint16_t           width;     /* screen size in pixels, boxes must lie inside it */
    uint16_t           height;
    uint8_t            scale;     /* 1 to TEXTDRAW_MAX_SCALE */
} textdraw_config_t;

typedef struct textdraw {
    textdraw_config_t config;
    bool              ready;
    uint16_t          cell[TEXTDRAW_CELL_PIXELS] __attribute__((aligned(4)));   /* private */
} textdraw_t;

/* Prepares the module. The textdraw_t may hold anything before the call and must not move after it. */
int textdraw_init(textdraw_t *td, const textdraw_config_t *config);

/* Width and height of one glyph cell in pixels at the configured scale. 0 when not initialised. */
uint16_t textdraw_cell_width(const textdraw_t *td);
uint16_t textdraw_cell_height(const textdraw_t *td);

/* Number of code points in a UTF-8 string, not counting new line and carriage return. Bad bytes count one each. */
size_t textdraw_count_glyphs(const char *utf8);

/*
 * Draws text inside the box that starts at x, y and is w_px wide and h_px high. Only whole cells
 * are used: the box holds (w_px / cell width) columns and (h_px / cell height) rows. Each cell is
 * fully painted (glyph in fg, the rest in bg), but the space after the last character is not
 * touched, so the caller clears the box first when old text must go. Returns TEXTDRAW_OK,
 * TEXTDRAW_TRUNCATED when the text was cut at the end of the box, or a negative error code.
 * *drawn (optional, may be NULL) receives the number of cells drawn.
 */
int textdraw_draw(textdraw_t *td, uint16_t x, uint16_t y, uint16_t w_px, uint16_t h_px,
                  const char *utf8, uint16_t fg, uint16_t bg, size_t *drawn);

#ifdef __cplusplus
}
#endif

#endif /* TEXTDRAW_H */