/* Core/Inc/big_freq.h */

#ifndef BIG_FREQ_H
#define BIG_FREQ_H

#include <stdint.h>

/* big 7-segment frequency renderer for SH1106.
 *
 * digit bounding box: SEG_W x SEG_H pixels.
 * segment layout:
 *
 *    AAA
 *   F   B
 *   F   B
 *    GGG
 *   E   C
 *   E   C
 *    DDD  ..
 *
 * call Draw_BigFreq(freq_mhz, x0, y0) to render a frequency value
 * (in millihertz) starting at pixel (x0, y0), horizontally centred
 * within a 128-pixel-wide display if x0 is computed by the caller. */

#define SEG_W        16u   /* digit bounding box width           */
#define SEG_H        28u   /* digit bounding box height          */
#define SEG_GAP       2u   /* gap between adjacent digit boxes   */
#define SEG_DOT_W     4u   /* decimal point slot width           */
#define SEG_DOT_GAP   2u   /* gap after decimal point            */

/* draws one 7-segment digit (0-9) with its top-left corner at (x, y) */
void Draw_BigDigit(uint8_t x, uint8_t y, uint8_t digit);

/* draws a decimal point aligned to the bottom of a digit at (x, y) */
void Draw_BigDot(uint8_t x, uint8_t y);

/* renders freq_mhz (millihertz) as big 7-segment digits + small "Hz" suffix.
 * the number is drawn starting at pixel (x0, y0).
 * use Big_FreqWidth(freq_mhz) to measure the total width first if you need
 * to centre the result. */
void Draw_BigFreq(uint32_t freq_mhz, uint8_t x0, uint8_t y0);

/* returns the total pixel width that Draw_BigFreq would occupy */
uint8_t Big_FreqWidth(uint32_t freq_mhz);

#endif /* BIG_FREQ_H */
