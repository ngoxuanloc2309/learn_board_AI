#include "textdraw.h"

#define CODE_REPLACEMENT   0xFFFDUL

/* Hollow box for a code point that is not in the font. Drawn by this file, not taken from any font. */
static const uint8_t s_box_rows[FONT_VI_GLYPH_H] = {
    0x00, 0x00, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00, 0x00
};

/*
 * Reads one code point and moves *s past it. Bad input (stray continuation byte, overlong form,
 * surrogate, value above U+10FFFF, or a sequence cut by the end of the string) gives
 * CODE_REPLACEMENT and moves one byte. The end of the string gives 0.
 */
static uint32_t utf8_next(const uint8_t **s)
{
    const uint8_t *p = *s;
    uint8_t b0 = p[0];
    uint32_t cp;
    unsigned need;
    unsigned i;

    if (b0 == 0U) {
        return 0U;
    }
    if (b0 < 0x80U) {
        *s = p + 1;
        return b0;
    }
    if ((b0 >= 0xC2U) && (b0 <= 0xDFU)) {
        need = 1U;
        cp = (uint32_t)(b0 & 0x1FU);
    } else if ((b0 >= 0xE0U) && (b0 <= 0xEFU)) {
        need = 2U;
        cp = (uint32_t)(b0 & 0x0FU);
    } else if ((b0 >= 0xF0U) && (b0 <= 0xF4U)) {
        need = 3U;
        cp = (uint32_t)(b0 & 0x07U);
    } else {
        *s = p + 1;
        return CODE_REPLACEMENT;
    }
    for (i = 1U; i <= need; i++) {
        if ((p[i] & 0xC0U) != 0x80U) {   /* also true for the end of the string (0) */
            *s = p + 1;
            return CODE_REPLACEMENT;
        }
        cp = (cp << 6) | (uint32_t)(p[i] & 0x3FU);
    }
    if (((need == 2U) && (cp < 0x800UL)) || ((need == 3U) && (cp < 0x10000UL)) ||
        ((cp >= 0xD800UL) && (cp <= 0xDFFFUL)) || (cp > 0x10FFFFUL)) {
        *s = p + 1;
        return CODE_REPLACEMENT;
    }
    *s = p + need + 1U;
    return cp;
}

/* Binary search in the sorted font table. Returns the 16 rows or the box when the code is missing. */
static const uint8_t *find_rows(uint32_t cp)
{
    size_t lo = 0U;
    size_t hi = FONT_VI_GLYPH_COUNT;

    if (cp <= 0xFFFFUL) {
        while (lo < hi) {
            size_t mid = lo + ((hi - lo) / 2U);
            uint32_t code = g_font_vi_8x16[mid].code;

            if (code == cp) {
                return g_font_vi_8x16[mid].rows;
            }
            if (code < cp) {
                lo = mid + 1U;
            } else {
                hi = mid;
            }
        }
    }
    return s_box_rows;
}

static void render_cell(textdraw_t *td, const uint8_t *rows, uint16_t fg, uint16_t bg)
{
    const unsigned scale = td->config.scale;
    const unsigned cell_w = FONT_VI_GLYPH_W * scale;
    unsigned row;
    unsigned col;
    unsigned dy;
    unsigned dx;

    for (row = 0U; row < FONT_VI_GLYPH_H; row++) {
        for (dy = 0U; dy < scale; dy++) {
            uint16_t *line = &td->cell[((row * scale) + dy) * cell_w];

            for (col = 0U; col < FONT_VI_GLYPH_W; col++) {
                uint16_t color = (((rows[row] >> (7U - col)) & 1U) != 0U) ? fg : bg;

                for (dx = 0U; dx < scale; dx++) {
                    line[(col * scale) + dx] = color;
                }
            }
        }
    }
}

int textdraw_init(textdraw_t *td, const textdraw_config_t *config)
{
    if ((td == NULL) || (config == NULL) || (config->blit == NULL) || (config->width == 0U) ||
        (config->height == 0U) || (config->scale < 1U) || (config->scale > TEXTDRAW_MAX_SCALE)) {
        return TEXTDRAW_ERR_PARAM;
    }
    td->config = *config;
    td->ready = true;
    return TEXTDRAW_OK;
}

uint16_t textdraw_cell_width(const textdraw_t *td)
{
    return ((td != NULL) && td->ready) ? (uint16_t)(FONT_VI_GLYPH_W * td->config.scale) : 0U;
}

uint16_t textdraw_cell_height(const textdraw_t *td)
{
    return ((td != NULL) && td->ready) ? (uint16_t)(FONT_VI_GLYPH_H * td->config.scale) : 0U;
}

size_t textdraw_count_glyphs(const char *utf8)
{
    const uint8_t *p = (const uint8_t *)utf8;
    size_t n = 0U;

    if (p == NULL) {
        return 0U;
    }
    for (;;) {
        uint32_t cp = utf8_next(&p);

        if (cp == 0U) {
            break;
        }
        if ((cp != (uint32_t)'\n') && (cp != (uint32_t)'\r')) {
            n++;
        }
    }
    return n;
}

int textdraw_draw(textdraw_t *td, uint16_t x, uint16_t y, uint16_t w_px, uint16_t h_px,
                  const char *utf8, uint16_t fg, uint16_t bg, size_t *drawn)
{
    const uint8_t *p = (const uint8_t *)utf8;
    unsigned cell_w;
    unsigned cell_h;
    unsigned cols;
    unsigned rows;
    unsigned col = 0U;
    unsigned row = 0U;
    size_t count = 0U;
    int result = TEXTDRAW_OK;

    if (drawn != NULL) {
        *drawn = 0U;
    }
    if ((td == NULL) || (utf8 == NULL)) {
        return TEXTDRAW_ERR_PARAM;
    }
    if (!td->ready) {
        return TEXTDRAW_ERR_STATE;
    }
    cell_w = FONT_VI_GLYPH_W * td->config.scale;
    cell_h = FONT_VI_GLYPH_H * td->config.scale;
    cols = w_px / cell_w;
    rows = h_px / cell_h;
    if ((cols == 0U) || (rows == 0U) || (((unsigned)x + (cols * cell_w)) > td->config.width) ||
        (((unsigned)y + (rows * cell_h)) > td->config.height)) {
        return TEXTDRAW_ERR_PARAM;
    }

    for (;;) {
        uint32_t cp = utf8_next(&p);

        if (cp == 0U) {
            break;
        }
        if (cp == (uint32_t)'\r') {
            continue;
        }
        if (cp == (uint32_t)'\n') {
            col = 0U;
            row++;
            if (row >= rows) {
                result = (*p != 0U) ? TEXTDRAW_TRUNCATED : TEXTDRAW_OK;
                break;
            }
            continue;
        }
        if (col >= cols) {
            col = 0U;
            row++;
        }
        if (row >= rows) {
            result = TEXTDRAW_TRUNCATED;
            break;
        }
        render_cell(td, find_rows(cp), fg, bg);
        {
            unsigned px = (unsigned)x + (col * cell_w);
            unsigned py = (unsigned)y + (row * cell_h);

            if (td->config.blit(td->config.ctx, (uint16_t)px, (uint16_t)py,
                                (uint16_t)(px + cell_w - 1U), (uint16_t)(py + cell_h - 1U),
                                td->cell, (size_t)cell_w * cell_h * sizeof(uint16_t)) != 0) {
                result = TEXTDRAW_ERR_BLIT;
                break;
            }
        }
        col++;
        count++;
    }
    if (drawn != NULL) {
        *drawn = count;
    }
    return result;
}