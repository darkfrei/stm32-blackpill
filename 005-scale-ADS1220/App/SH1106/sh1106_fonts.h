/**
 * @file    sh1106_fonts.h
 * @brief   Font definitions for SH1106 OLED Display Driver
 */

#ifndef __SH1106_FONTS_H__
#define __SH1106_FONTS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sh1106.h"

/* ========================================================================
 * FONT DECLARATIONS
 * ======================================================================== */

#ifdef SH1106_INCLUDE_FONT_6x8
    /**
     * @brief 6x8 pixel monospace font
     */
    extern const SH1106_Font_t Font_6x8;
#endif

#ifdef SH1106_INCLUDE_FONT_7x10
    /**
     * @brief 7x10 pixel monospace font
     */
    extern const SH1106_Font_t Font_7x10;
#endif

#ifdef SH1106_INCLUDE_FONT_11x18
    /**
     * @brief 11x18 pixel monospace font
     */
    extern const SH1106_Font_t Font_11x18;
#endif

#ifdef SH1106_INCLUDE_FONT_8H
    /**
     * @brief 8 pixel height proportional font
     * Custom font with variable character widths
     */
    extern const SH1106_Font_t Font_8H;
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SH1106_FONTS_H__ */
