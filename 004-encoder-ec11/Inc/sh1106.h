/**
 * @file    sh1106.h
 * @brief   SH1106 OLED Display Driver for STM32
 * @author  
 * @version 1.0
 * @date    2025
 */

#ifndef __SH1106_H__
#define __SH1106_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "sh1106_conf.h"

/* ========================================================================
 * TYPE DEFINITIONS
 * ======================================================================== */

/**
 * @brief Status codes for function returns
 */
typedef enum {
    SH1106_OK = 0,      /**< Success */
    SH1106_ERROR = 1    /**< Error occurred */
} SH1106_Status_t;

/**
 * @brief Color enumeration for monochrome display
 */
typedef enum {
    SH1106_COLOR_BLACK = 0,  /**< Pixel off */
    SH1106_COLOR_WHITE = 1   /**< Pixel on */
} SH1106_COLOR_t;

/**
 * @brief Font structure for character rendering
 */
typedef struct {
    uint8_t width;              /**< Maximum character width in pixels */
    uint8_t height;             /**< Font height in pixels */
    const uint16_t *data;       /**< Pointer to font data array */
    const uint8_t *char_width;  /**< Pointer to character width array (NULL for monospace) */
} SH1106_Font_t;

/**
 * @brief Display structure containing state information
 */
typedef struct {
    uint16_t current_x;         /**< Current X position for text cursor */
    uint16_t current_y;         /**< Current Y position for text cursor */
    bool initialized;           /**< Initialization flag */
    bool inverted;              /**< Color inversion flag */
} SH1106_t;

/* ========================================================================
 * FUNCTION PROTOTYPES - BASIC OPERATIONS
 * ======================================================================== */

/**
 * @brief Initialize the SH1106 display
 * @return SH1106_OK if successful, SH1106_ERROR otherwise
 */
SH1106_Status_t SH1106_Init(void);

/**
 * @brief Fill entire screen with specified color
 * @param color Color to fill (BLACK or WHITE)
 */
void SH1106_Fill(SH1106_COLOR_t color);

/**
 * @brief Update display with buffer contents
 * @note This function sends the entire buffer to the display
 */
void SH1106_UpdateScreen(void);

/**
 * @brief Update a portion of the screen (incremental update)
 * @param chunk Chunk number to update (0 to num_chunks-1)
 * @return true if more chunks to update, false if done
 */
bool SH1106_UpdateScreenChunk(uint16_t chunk);

/**
 * @brief Get total number of chunks for incremental update
 * @return Number of chunks
 */
uint16_t SH1106_GetTotalChunks(void);

/**
 * @brief Turn display on
 */
void SH1106_ON(void);

/**
 * @brief Turn display off
 */
void SH1106_OFF(void);

/**
 * @brief Toggle color inversion
 */
void SH1106_ToggleInvert(void);

/**
 * @brief Set display brightness
 * @param value Brightness value (0-255)
 */
void SH1106_SetBrightness(uint8_t value);

/* ========================================================================
 * FUNCTION PROTOTYPES - DRAWING PRIMITIVES
 * ======================================================================== */

/**
 * @brief Draw a single pixel
 * @param x X coordinate (0 to WIDTH-1)
 * @param y Y coordinate (0 to HEIGHT-1)
 * @param color Pixel color
 */
void SH1106_DrawPixel(uint8_t x, uint8_t y, SH1106_COLOR_t color);

/**
 * @brief Draw a line from (x0,y0) to (x1,y1)
 * @param x0 Start X coordinate
 * @param y0 Start Y coordinate
 * @param x1 End X coordinate
 * @param y1 End Y coordinate
 * @param color Line color
 */
void SH1106_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, SH1106_COLOR_t color);

/**
 * @brief Draw a rectangle outline
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param w Width
 * @param h Height
 * @param color Rectangle color
 */
void SH1106_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, SH1106_COLOR_t color);

/**
 * @brief Draw a filled rectangle
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param w Width
 * @param h Height
 * @param color Fill color
 */
void SH1106_FillRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, SH1106_COLOR_t color);

/**
 * @brief Draw a circle outline
 * @param x0 Center X coordinate
 * @param y0 Center Y coordinate
 * @param r Radius
 * @param color Circle color
 */
void SH1106_DrawCircle(uint8_t x0, uint8_t y0, uint8_t r, SH1106_COLOR_t color);

/**
 * @brief Draw a filled circle
 * @param x0 Center X coordinate
 * @param y0 Center Y coordinate
 * @param r Radius
 * @param color Fill color
 */
void SH1106_FillCircle(uint8_t x0, uint8_t y0, uint8_t r, SH1106_COLOR_t color);

/**
 * @brief Draw a bitmap image
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param bitmap Pointer to bitmap data
 * @param w Width of bitmap
 * @param h Height of bitmap
 * @param color Foreground color
 */
void SH1106_DrawBitmap(uint8_t x, uint8_t y, const uint8_t* bitmap, uint8_t w, uint8_t h, SH1106_COLOR_t color);

/* ========================================================================
 * FUNCTION PROTOTYPES - TEXT RENDERING
 * ======================================================================== */

/**
 * @brief Set cursor position for text
 * @param x X coordinate
 * @param y Y coordinate
 */
void SH1106_SetCursor(uint8_t x, uint8_t y);

/**
 * @brief Get current cursor position
 * @param x Pointer to store X coordinate
 * @param y Pointer to store Y coordinate
 */
void SH1106_GetCursor(uint16_t* x, uint16_t* y);

/**
 * @brief Write a single character at current cursor position
 * @param ch Character to write
 * @param font Font to use
 * @param color Text color
 * @return Character written (0 if failed)
 */
char SH1106_WriteChar(char ch, SH1106_Font_t font, SH1106_COLOR_t color);

/**
 * @brief Write a string at current cursor position
 * @param str String to write
 * @param font Font to use
 * @param color Text color
 * @return Number of characters written
 */
uint16_t SH1106_WriteString(const char* str, SH1106_Font_t font, SH1106_COLOR_t color);

/**
 * @brief Write a string at specific position
 * @param x X coordinate
 * @param y Y coordinate
 * @param str String to write
 * @param font Font to use
 * @param color Text color
 * @return Number of characters written
 */
uint16_t SH1106_WriteStringAt(uint8_t x, uint8_t y, const char* str, SH1106_Font_t font, SH1106_COLOR_t color);

/**
 * @brief Calculate pixel width of a string
 * @param str String to measure
 * @param font Font to use
 * @return Width in pixels
 */
uint16_t SH1106_GetStringWidth(const char* str, SH1106_Font_t font);

/* ========================================================================
 * FUNCTION PROTOTYPES - BUFFER ACCESS
 * ======================================================================== */

/**
 * @brief Get pointer to display buffer
 * @return Pointer to buffer
 */
uint8_t* SH1106_GetBuffer(void);

/**
 * @brief Clear display buffer
 */
void SH1106_Clear(void);

/* ========================================================================
 * LOW-LEVEL FUNCTIONS (Internal use)
 * ======================================================================== */

/**
 * @brief Write command to SH1106
 * @param cmd Command byte
 */
void SH1106_WriteCommand(uint8_t cmd);

/**
 * @brief Write data to SH1106
 * @param data Pointer to data buffer
 * @param len Length of data
 */
void SH1106_WriteData(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __SH1106_H__ */
