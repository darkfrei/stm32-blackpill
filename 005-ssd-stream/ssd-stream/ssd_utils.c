/**
 * @file ssd_utils.c
 * @brief Implementation of utility functions for SSD1306 driver
 * 
 * @author Claude (Anthropic AI)
 * @date 2026-01-31
 */

#include "ssd_utils.h"
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Graphics Primitives Implementation
 * ================================================================ */

void ssd_draw_hline(uint8_t x, uint8_t y, uint8_t width, uint8_t color)
{
    for (uint8_t i = 0; i < width; i++) {
        ssd_set_pixel(x + i, y, color);
    }
}

void ssd_draw_vline(uint8_t x, uint8_t y, uint8_t height, uint8_t color)
{
    for (uint8_t i = 0; i < height; i++) {
        ssd_set_pixel(x, y + i, color);
    }
}

void ssd_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color)
{
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    
    while (1) {
        ssd_set_pixel(x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void ssd_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    ssd_draw_hline(x, y, w, color);
    ssd_draw_hline(x, y + h - 1, w, color);
    ssd_draw_vline(x, y, h, color);
    ssd_draw_vline(x + w - 1, y, h, color);
}

void ssd_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    for (uint8_t i = 0; i < h; i++) {
        ssd_draw_hline(x, y + i, w, color);
    }
}

void ssd_draw_circle(uint8_t x0, uint8_t y0, uint8_t radius, uint8_t color)
{
    int16_t f = 1 - radius;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * radius;
    int16_t x = 0;
    int16_t y = radius;
    
    ssd_set_pixel(x0, y0 + radius, color);
    ssd_set_pixel(x0, y0 - radius, color);
    ssd_set_pixel(x0 + radius, y0, color);
    ssd_set_pixel(x0 - radius, y0, color);
    
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        
        ssd_set_pixel(x0 + x, y0 + y, color);
        ssd_set_pixel(x0 - x, y0 + y, color);
        ssd_set_pixel(x0 + x, y0 - y, color);
        ssd_set_pixel(x0 - x, y0 - y, color);
        ssd_set_pixel(x0 + y, y0 + x, color);
        ssd_set_pixel(x0 - y, y0 + x, color);
        ssd_set_pixel(x0 + y, y0 - x, color);
        ssd_set_pixel(x0 - y, y0 - x, color);
    }
}

void ssd_fill_circle(uint8_t x0, uint8_t y0, uint8_t radius, uint8_t color)
{
    int16_t f = 1 - radius;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * radius;
    int16_t x = 0;
    int16_t y = radius;
    
    ssd_draw_vline(x0, y0 - radius, 2 * radius + 1, color);
    
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        
        ssd_draw_vline(x0 + x, y0 - y, 2 * y + 1, color);
        ssd_draw_vline(x0 - x, y0 - y, 2 * y + 1, color);
        ssd_draw_vline(x0 + y, y0 - x, 2 * x + 1, color);
        ssd_draw_vline(x0 - y, y0 - x, 2 * x + 1, color);
    }
}

void ssd_draw_triangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                       uint8_t x2, uint8_t y2, uint8_t color)
{
    ssd_draw_line(x0, y0, x1, y1, color);
    ssd_draw_line(x1, y1, x2, y2, color);
    ssd_draw_line(x2, y2, x0, y0, color);
}

void ssd_fill_triangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                       uint8_t x2, uint8_t y2, uint8_t color)
{
    /* Sort vertices by Y */
    if (y0 > y1) { 
        uint8_t t; 
        t = y0; y0 = y1; y1 = t;
        t = x0; x0 = x1; x1 = t;
    }
    if (y1 > y2) { 
        uint8_t t; 
        t = y2; y2 = y1; y1 = t;
        t = x2; x2 = x1; x1 = t;
    }
    if (y0 > y1) { 
        uint8_t t; 
        t = y0; y0 = y1; y1 = t;
        t = x0; x0 = x1; x1 = t;
    }
    
    /* Draw horizontal lines */
    for (int16_t y = y0; y <= y2; y++) {
        int16_t xa, xb;
        
        if (y < y1) {
            xa = x0 + (x1 - x0) * (y - y0) / (y1 - y0 + 1);
            xb = x0 + (x2 - x0) * (y - y0) / (y2 - y0 + 1);
        } else {
            xa = x1 + (x2 - x1) * (y - y1) / (y2 - y1 + 1);
            xb = x0 + (x2 - x0) * (y - y0) / (y2 - y0 + 1);
        }
        
        if (xa > xb) {
            int16_t t = xa;
            xa = xb;
            xb = t;
        }
        
        ssd_draw_hline(xa, y, xb - xa + 1, color);
    }
}

/* ================================================================
 * Bitmap Operations
 * ================================================================ */

void ssd_draw_bitmap(uint8_t x, uint8_t y, const uint8_t *bitmap,
                     uint8_t w, uint8_t h, uint8_t color)
{
    for (uint8_t j = 0; j < h; j++) {
        for (uint8_t i = 0; i < w; i++) {
            uint8_t byte = bitmap[j * ((w + 7) / 8) + i / 8];
            if (byte & (0x80 >> (i % 8))) {
                ssd_set_pixel(x + i, y + j, color);
            }
        }
    }
}

void ssd_draw_xbm(uint8_t x, uint8_t y, const uint8_t *xbm_bits,
                  uint8_t w, uint8_t h)
{
    for (uint8_t j = 0; j < h; j++) {
        for (uint8_t i = 0; i < w; i++) {
            uint8_t byte = xbm_bits[j * ((w + 7) / 8) + i / 8];
            if (byte & (1 << (i % 8))) {  /* XBM is LSB first */
                ssd_set_pixel(x + i, y + j, 1);
            }
        }
    }
}

/* ================================================================
 * Advanced Drawing
 * ================================================================ */

void ssd_scroll_horizontal(int16_t pixels)
{
    uint8_t *fb = ssd_get_framebuffer();
    
    if (pixels == 0) return;
    
    if (pixels > 0) {
        /* Scroll right */
        for (int16_t page = 0; page < 8; page++) {
            uint16_t offset = page * 128;
            memmove(&fb[offset + pixels], &fb[offset], 128 - pixels);
            memset(&fb[offset], 0, pixels);
        }
    } else {
        /* Scroll left */
        pixels = -pixels;
        for (int16_t page = 0; page < 8; page++) {
            uint16_t offset = page * 128;
            memmove(&fb[offset], &fb[offset + pixels], 128 - pixels);
            memset(&fb[offset + 128 - pixels], 0, pixels);
        }
    }
}

void ssd_scroll_vertical(int16_t pixels)
{
    uint8_t *fb = ssd_get_framebuffer();
    
    if (pixels == 0) return;
    
    if (pixels > 0) {
        /* Scroll down */
        int16_t pages = pixels / 8;
        int16_t bits = pixels % 8;
        
        /* Shift pages */
        if (pages > 0) {
            memmove(&fb[pages * 128], fb, (8 - pages) * 128);
            memset(fb, 0, pages * 128);
        }
        
        /* Shift bits */
        if (bits > 0) {
            for (int16_t i = 1023; i >= 0; i--) {
                fb[i] = (fb[i] << bits);
                if (i >= 128) {
                    fb[i] |= (fb[i - 128] >> (8 - bits));
                }
            }
        }
    } else {
        /* Scroll up */
        pixels = -pixels;
        int16_t pages = pixels / 8;
        int16_t bits = pixels % 8;
        
        if (pages > 0) {
            memmove(fb, &fb[pages * 128], (8 - pages) * 128);
            memset(&fb[(8 - pages) * 128], 0, pages * 128);
        }
        
        if (bits > 0) {
            for (int16_t i = 0; i < 1024; i++) {
                fb[i] = (fb[i] >> bits);
                if (i < 896) {  /* 1024 - 128 */
                    fb[i] |= (fb[i + 128] << (8 - bits));
                }
            }
        }
    }
}

void ssd_invert(void)
{
    uint8_t *fb = ssd_get_framebuffer();
    for (uint16_t i = 0; i < 1024; i++) {
        fb[i] = ~fb[i];
    }
}

void ssd_draw_progress_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                           uint8_t percent)
{
    if (percent > 100) percent = 100;
    
    /* Draw border */
    ssd_draw_rect(x, y, w, h, 1);
    
    /* Fill based on percentage */
    uint8_t fill_w = ((w - 2) * percent) / 100;
    if (fill_w > 0) {
        ssd_fill_rect(x + 1, y + 1, fill_w, h - 2, 1);
    }
}

void ssd_draw_graph(uint8_t x, uint8_t y, const uint8_t *data,
                    uint8_t len, uint8_t max_val, uint8_t h)
{
    if (max_val == 0) max_val = 1;  /* Avoid division by zero */
    
    for (uint8_t i = 0; i < len - 1; i++) {
        uint8_t y1 = y + h - ((data[i] * h) / max_val);
        uint8_t y2 = y + h - ((data[i + 1] * h) / max_val);
        
        ssd_draw_line(x + i, y1, x + i + 1, y2, 1);
    }
}

/* ================================================================
 * Simple 5x7 Font Data (ASCII 32-126)
 * ================================================================ */

const uint8_t ssd_font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* Space (32) */
    {0x00, 0x00, 0x5F, 0x00, 0x00}, /* ! */
    {0x00, 0x07, 0x00, 0x07, 0x00}, /* " */
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, /* # */
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, /* $ */
    {0x23, 0x13, 0x08, 0x64, 0x62}, /* % */
    {0x36, 0x49, 0x55, 0x22, 0x50}, /* & */
    {0x00, 0x05, 0x03, 0x00, 0x00}, /* ' */
    {0x00, 0x1C, 0x22, 0x41, 0x00}, /* ( */
    {0x00, 0x41, 0x22, 0x1C, 0x00}, /* ) */
    {0x14, 0x08, 0x3E, 0x08, 0x14}, /* * */
    {0x08, 0x08, 0x3E, 0x08, 0x08}, /* + */
    {0x00, 0x50, 0x30, 0x00, 0x00}, /* , */
    {0x08, 0x08, 0x08, 0x08, 0x08}, /* - */
    {0x00, 0x60, 0x60, 0x00, 0x00}, /* . */
    {0x20, 0x10, 0x08, 0x04, 0x02}, /* / */
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 0 (48) */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 2 */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 6 */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /* 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 8 */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 9 */
    {0x00, 0x36, 0x36, 0x00, 0x00}, /* : (58) */
    {0x00, 0x56, 0x36, 0x00, 0x00}, /* ; */
    {0x08, 0x14, 0x22, 0x41, 0x00}, /* < */
    {0x14, 0x14, 0x14, 0x14, 0x14}, /* = */
    {0x00, 0x41, 0x22, 0x14, 0x08}, /* > */
    {0x02, 0x01, 0x51, 0x09, 0x06}, /* ? */
    {0x32, 0x49, 0x79, 0x41, 0x3E}, /* @ */
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* A (65) */
    {0x7F, 0x49, 0x49, 0x49, 0x36}, /* B */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, /* C */
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* D */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, /* E */
    {0x7F, 0x09, 0x09, 0x09, 0x01}, /* F */
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, /* G */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* H */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /* I */
    {0x20, 0x40, 0x41, 0x3F, 0x01}, /* J */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, /* K */
    {0x7F, 0x40, 0x40, 0x40, 0x40}, /* L */
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, /* M */
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* N */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* O */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /* P */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, /* Q */
    {0x7F, 0x09, 0x19, 0x29, 0x46}, /* R */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /* S */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /* T */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, /* U */
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* V */
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, /* W */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* X */
    {0x07, 0x08, 0x70, 0x08, 0x07}, /* Y */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /* Z (90) */
    {0x00, 0x7F, 0x41, 0x41, 0x00}, /* [ */
    {0x02, 0x04, 0x08, 0x10, 0x20}, /* \ */
    {0x00, 0x41, 0x41, 0x7F, 0x00}, /* ] */
    {0x04, 0x02, 0x01, 0x02, 0x04}, /* ^ */
    {0x40, 0x40, 0x40, 0x40, 0x40}, /* _ */
};

void ssd_draw_char(uint8_t x, uint8_t y, char c, const uint8_t *font,
                   uint8_t size, uint8_t color)
{
    if (c < 32 || c > 95) return;  /* Only printable ASCII */
    
    const uint8_t *glyph = ssd_font_5x7[c - 32];
    
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (uint8_t bit = 0; bit < 7; bit++) {
            if (line & (1 << bit)) {
                if (size == 1) {
                    ssd_set_pixel(x + col, y + bit, color);
                } else {
                    ssd_fill_rect(x + col * size, y + bit * size, 
                                  size, size, color);
                }
            }
        }
    }
}

void ssd_draw_string(uint8_t x, uint8_t y, const char *str,
                     const uint8_t *font, uint8_t size, uint8_t color)
{
    uint8_t cursor_x = x;
    
    while (*str) {
        ssd_draw_char(cursor_x, y, *str, font, size, color);
        cursor_x += (5 + 1) * size;  /* 5 pixel width + 1 spacing */
        str++;
    }
}

uint16_t ssd_string_width(const char *str, const uint8_t *font, uint8_t size)
{
    uint16_t width = 0;
    while (*str) {
        width += (5 + 1) * size;
        str++;
    }
    return width > 0 ? width - size : 0;  /* Remove trailing space */
}
