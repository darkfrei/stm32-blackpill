/**
 * @file ssd_utils.h
 * @brief Utility functions for SSD1306 driver - graphics primitives and helpers
 * 
 * This file provides higher-level drawing functions built on top of the
 * streaming driver. Include this for convenient graphics operations.
 * 
 * @author Claude (Anthropic AI)
 * @date 2026-01-31
 * @version 1.0
 */

#ifndef SSD_UTILS_H
#define SSD_UTILS_H

#include "ssd_stream.h"
#include <stdint.h>

/* ================================================================
 * Graphics Primitives
 * ================================================================ */

/**
 * @brief Draw a horizontal line
 * 
 * @param x Starting X coordinate
 * @param y Y coordinate
 * @param width Line width in pixels
 * @param color 1 = white, 0 = black
 */
void ssd_draw_hline(uint8_t x, uint8_t y, uint8_t width, uint8_t color);

/**
 * @brief Draw a vertical line
 * 
 * @param x X coordinate
 * @param y Starting Y coordinate
 * @param height Line height in pixels
 * @param color 1 = white, 0 = black
 */
void ssd_draw_vline(uint8_t x, uint8_t y, uint8_t height, uint8_t color);

/**
 * @brief Draw a line using Bresenham's algorithm
 * 
 * @param x0 Starting X coordinate
 * @param y0 Starting Y coordinate
 * @param x1 Ending X coordinate
 * @param y1 Ending Y coordinate
 * @param color 1 = white, 0 = black
 */
void ssd_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);

/**
 * @brief Draw a rectangle outline
 * 
 * @param x Top-left X coordinate
 * @param y Top-left Y coordinate
 * @param w Width in pixels
 * @param h Height in pixels
 * @param color 1 = white, 0 = black
 */
void ssd_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);

/**
 * @brief Draw a filled rectangle
 * 
 * @param x Top-left X coordinate
 * @param y Top-left Y coordinate
 * @param w Width in pixels
 * @param h Height in pixels
 * @param color 1 = white, 0 = black
 */
void ssd_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);

/**
 * @brief Draw a circle outline (Midpoint circle algorithm)
 * 
 * @param x0 Center X coordinate
 * @param y0 Center Y coordinate
 * @param radius Circle radius in pixels
 * @param color 1 = white, 0 = black
 */
void ssd_draw_circle(uint8_t x0, uint8_t y0, uint8_t radius, uint8_t color);

/**
 * @brief Draw a filled circle
 * 
 * @param x0 Center X coordinate
 * @param y0 Center Y coordinate
 * @param radius Circle radius in pixels
 * @param color 1 = white, 0 = black
 */
void ssd_fill_circle(uint8_t x0, uint8_t y0, uint8_t radius, uint8_t color);

/**
 * @brief Draw a triangle outline
 * 
 * @param x0 First vertex X
 * @param y0 First vertex Y
 * @param x1 Second vertex X
 * @param y1 Second vertex Y
 * @param x2 Third vertex X
 * @param y2 Third vertex Y
 * @param color 1 = white, 0 = black
 */
void ssd_draw_triangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, 
                       uint8_t x2, uint8_t y2, uint8_t color);

/**
 * @brief Draw a filled triangle
 * 
 * @param x0 First vertex X
 * @param y0 First vertex Y
 * @param x1 Second vertex X
 * @param y1 Second vertex Y
 * @param x2 Third vertex X
 * @param y2 Third vertex Y
 * @param color 1 = white, 0 = black
 */
void ssd_fill_triangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                       uint8_t x2, uint8_t y2, uint8_t color);

/* ================================================================
 * Bitmap Operations
 * ================================================================ */

/**
 * @brief Draw a bitmap (1-bit per pixel, MSB first)
 * 
 * @param x Top-left X coordinate
 * @param y Top-left Y coordinate  
 * @param bitmap Pointer to bitmap data
 * @param w Width in pixels
 * @param h Height in pixels
 * @param color Foreground color (1 = white, 0 = black)
 */
void ssd_draw_bitmap(uint8_t x, uint8_t y, const uint8_t *bitmap, 
                     uint8_t w, uint8_t h, uint8_t color);

/**
 * @brief Draw XBM format bitmap (LSB first)
 * 
 * @param x Top-left X coordinate
 * @param y Top-left Y coordinate
 * @param xbm_bits Pointer to XBM bitmap data
 * @param w Width in pixels
 * @param h Height in pixels
 */
void ssd_draw_xbm(uint8_t x, uint8_t y, const uint8_t *xbm_bits,
                  uint8_t w, uint8_t h);

/* ================================================================
 * Text Rendering (requires font data)
 * ================================================================ */

/**
 * @brief Draw a single character
 * 
 * @param x Top-left X coordinate
 * @param y Top-left Y coordinate
 * @param c Character to draw (ASCII 32-95 supported)
 * @param font Pointer to font data (currently uses built-in 5x7 font)
 * @param size Font size multiplier (1 = normal, 2 = 2x, etc.)
 * @param color Foreground color (1 = white, 0 = black)
 */
void ssd_draw_char(uint8_t x, uint8_t y, char c, const uint8_t *font,
                   uint8_t size, uint8_t color);

/**
 * @brief Draw a string
 * 
 * @param x Starting X coordinate
 * @param y Starting Y coordinate
 * @param str String to draw (null-terminated)
 * @param font Pointer to font data (currently uses built-in 5x7 font)
 * @param size Font size multiplier
 * @param color Foreground color
 */
void ssd_draw_string(uint8_t x, uint8_t y, const char *str, 
                     const uint8_t *font, uint8_t size, uint8_t color);

/**
 * @brief Get string width in pixels
 * 
 * @param str String to measure
 * @param font Pointer to font data
 * @param size Font size multiplier
 * @return Width in pixels
 */
uint16_t ssd_string_width(const char *str, const uint8_t *font, uint8_t size);

/* ================================================================
 * Advanced Drawing
 * ================================================================ */

/**
 * @brief Scroll framebuffer horizontally
 * 
 * @param pixels Number of pixels to scroll (positive = right, negative = left)
 */
void ssd_scroll_horizontal(int16_t pixels);

/**
 * @brief Scroll framebuffer vertically
 * 
 * @param pixels Number of pixels to scroll (positive = down, negative = up)
 */
void ssd_scroll_vertical(int16_t pixels);

/**
 * @brief Invert display (swap black/white for all pixels)
 */
void ssd_invert(void);

/**
 * @brief Draw a progress bar
 * 
 * @param x Top-left X coordinate
 * @param y Top-left Y coordinate
 * @param w Width in pixels
 * @param h Height in pixels
 * @param percent Fill percentage (0-100)
 */
void ssd_draw_progress_bar(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                           uint8_t percent);

/**
 * @brief Draw a simple line graph/chart
 * 
 * @param x Top-left X coordinate
 * @param y Top-left Y coordinate
 * @param data Pointer to data array (values to plot)
 * @param len Length of data array
 * @param max_val Maximum value for Y-axis scaling
 * @param h Height of graph in pixels
 */
void ssd_draw_graph(uint8_t x, uint8_t y, const uint8_t *data, 
                    uint8_t len, uint8_t max_val, uint8_t h);

/* ================================================================
 * Simple 5x7 ASCII Font (included)
 * ================================================================ */

extern const uint8_t ssd_font_5x7[][5];

#define SSD_FONT_5X7_WIDTH  5
#define SSD_FONT_5X7_HEIGHT 7

#endif /* SSD_UTILS_H */
