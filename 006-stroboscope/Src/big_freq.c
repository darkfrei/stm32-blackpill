/* Core/Src/big_freq.c */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "sh1106.h"
#include "sh1106_fonts.h"
#include "big_freq.h"

/* segment bitmasks -- bit0=A(top) bit1=B(top-right) bit2=C(bot-right)
 * bit3=D(bottom) bit4=E(bot-left) bit5=F(top-left) bit6=G(middle) */
static const uint8_t seg_map[10] = {
    0x3F,   /* 0: A B C D E F     */
    0x06,   /* 1:   B C           */
    0x5B,   /* 2: A B   D E   G   */
    0x4F,   /* 3: A B C D     G   */
    0x66,   /* 4:   B C     F G   */
    0x6D,   /* 5: A   C D   F G   */
    0x7D,   /* 6: A   C D E F G   */
    0x07,   /* 7: A B C           */
    0x7F,   /* 8: A B C D E F G   */
    0x6F,   /* 9: A B C D   F G   */
};

/* SH1106_FillRectangle takes (x, y, width, height, color).
 * this wrapper accepts corner coordinates and converts to w/h,
 * with bounds checking to avoid overflow. */
static void fill_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    if (x1 > 127u || y1 > 63u) return;
    if (x2 > 127u) x2 = 127u;
    if (y2 > 63u)  y2 = 63u;
    if (x2 < x1 || y2 < y1) return;
    SH1106_FillRectangle((int16_t)x1, (uint8_t)y1,
                         (uint8_t)(x2 - x1 + 1u),
                         (uint8_t)(y2 - y1 + 1u),
                         SH1106_COLOR_WHITE);
}

/* pixel geometry inside a SEG_W x SEG_H bounding box:
 *
 *   top bar    : x+3 .. x+12,  y+0  .. y+2
 *   top-right  : x+13.. x+15,  y+3  .. y+12
 *   bot-right  : x+13.. x+15,  y+15 .. y+24
 *   bottom bar : x+3 .. x+12,  y+25 .. y+27
 *   bot-left   : x+0 .. x+2,   y+15 .. y+24
 *   top-left   : x+0 .. x+2,   y+3  .. y+12
 *   middle bar : x+3 .. x+12,  y+12 .. y+15  */

void Draw_BigDigit(uint8_t x, uint8_t y, uint8_t digit)
{
    if (digit > 9u) return;
    uint8_t  s  = seg_map[digit];
    uint16_t px = (uint16_t)x;
    uint16_t py = (uint16_t)y;

    if (s & 0x01u) fill_rect(px+3u,  py+0u,  px+12u, py+2u);
    if (s & 0x02u) fill_rect(px+13u, py+3u,  px+15u, py+12u);
    if (s & 0x04u) fill_rect(px+13u, py+15u, px+15u, py+24u);
    if (s & 0x08u) fill_rect(px+3u,  py+25u, px+12u, py+27u);
    if (s & 0x10u) fill_rect(px+0u,  py+15u, px+2u,  py+24u);
    if (s & 0x20u) fill_rect(px+0u,  py+3u,  px+2u,  py+12u);
    if (s & 0x40u) fill_rect(px+3u,  py+12u, px+12u, py+15u);
}

void Draw_BigDot(uint8_t x, uint8_t y)
{
    uint16_t px = (uint16_t)x;
    uint16_t py = (uint16_t)y;
    fill_rect(px, py+25u, px+2u, py+27u);
}

/* build a formatted string for freq_mhz.
 * buf must be at least 10 bytes.
 * carry-over rounding applied so e.g. 99950 mHz -> "100Hz". */
static void format_freq(uint32_t freq_mhz, char *buf, size_t bufsz)
{
    uint32_t hz = freq_mhz / 1000u;

    if (hz >= 100u) {
        snprintf(buf, bufsz, "%luHz", (freq_mhz + 500u) / 1000u);
    } else if (hz >= 10u) {
        uint32_t t = (freq_mhz + 50u) / 100u;
        uint32_t h = t / 10u;
        uint32_t d = t % 10u;
        if (h >= 100u)
            snprintf(buf, bufsz, "%luHz", h);
        else
            snprintf(buf, bufsz, "%lu.%luHz", h, d);
    } else if (hz >= 1u) {
        uint32_t c = (freq_mhz + 5u) / 10u;
        uint32_t h = c / 100u;
        uint32_t d = c % 100u;
        if (h >= 100u)
            snprintf(buf, bufsz, "%luHz", h);
        else if (h >= 10u)
            snprintf(buf, bufsz, "%lu.%luHz", h, d / 10u);
        else
            snprintf(buf, bufsz, "%lu.%02luHz", h, d);
    } else {
        uint32_t c = (freq_mhz + 5u) / 10u;
        if (c >= 100u)
            snprintf(buf, bufsz, "1.00Hz");
        else
            snprintf(buf, bufsz, "0.%02luHz", c);
    }
}

uint8_t Big_FreqWidth(uint32_t freq_mhz)
{
    char str[10];
    format_freq(freq_mhz, str, sizeof(str));

    uint8_t  len     = (uint8_t)strlen(str);
    uint8_t  num_len = (len >= 2u) ? (len - 2u) : 0u;
    uint16_t w       = 16u;   /* "Hz" suffix: 2 chars x 8 px */

    for (uint8_t i = 0u; i < num_len; i++) {
        if (str[i] >= '0' && str[i] <= '9')
            w += SEG_W + SEG_GAP;
        else if (str[i] == '.')
            w += SEG_DOT_W + SEG_DOT_GAP;
    }

    return (w <= 255u) ? (uint8_t)w : 255u;
}

void Draw_BigFreq(uint32_t freq_mhz, uint8_t x0, uint8_t y0)
{
    char str[10];
    format_freq(freq_mhz, str, sizeof(str));

    uint8_t len     = (uint8_t)strlen(str);
    uint8_t num_len = (len >= 2u) ? (len - 2u) : 0u;
    uint8_t cx      = x0;

    for (uint8_t i = 0u; i < num_len; i++) {
        if (str[i] >= '0' && str[i] <= '9') {
            Draw_BigDigit(cx, y0, (uint8_t)(str[i] - '0'));
            cx = (uint8_t)(cx + SEG_W + SEG_GAP);
        } else if (str[i] == '.') {
            Draw_BigDot(cx, y0);
            cx = (uint8_t)(cx + SEG_DOT_W + SEG_DOT_GAP);
        }
    }

    /* "Hz" baseline-aligned with the bottom of the big digits */
    if ((uint16_t)cx + 16u <= 128u)
        SH1106_WriteStringAt(cx, (uint8_t)((uint16_t)y0 + SEG_H - 8u),
                             "Hz", Font_8H, SH1106_COLOR_WHITE);
}