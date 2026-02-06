/**
 * @file ssd_stream.h
 * @brief Non-blocking streaming driver for SSD1306-compatible OLED displays
 * 
 * This driver implements a cyclic framebuffer streaming mechanism without DMA.
 * Each tick() call sends a fixed amount of data (1 to 1024 bytes) based on mode.
 * 
 * Hardware: STM32F411 BlackPill
 * Display: 128x64 monochrome OLED (SSD1306/SH1106 compatible)
 * Interface: I2C @ 400kHz (Fast-mode)
 * 
 * @note No DMA usage - uses blocking I2C transmissions only
 * @note Uses horizontal addressing mode for linear framebuffer access
 * 
 * @author Claude (Anthropic AI)
 * @date 2026-01-31
 * @version 1.0
 */

#ifndef SSD_STREAM_H
#define SSD_STREAM_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

/* Display dimensions */
#define SSD_WIDTH       128
#define SSD_HEIGHT      64
#define SSD_PAGES       8   /* 64 / 8 */
#define SSD_FB_SIZE     1024 /* 128 * 8 */

/* Mode range: 0..10 -> bytesPerTick = 1, 2, 4, ..., 1024 */
#define SSD_MODE_MIN    0
#define SSD_MODE_MAX    10

/* I2C control bytes */
#define SSD_CTRL_CMD    0x00  /* Command byte follows */
#define SSD_CTRL_DATA   0x40  /* Data byte(s) follow */

/* Default I2C timeout (ms) */
#define SSD_I2C_TIMEOUT 10

/**
 * @brief Driver state structure
 */
typedef struct {
    I2C_HandleTypeDef *hi2c;   /* I2C handle */
    uint16_t cursor;           /* Current position in framebuffer [0..1023] */
    uint8_t mode;              /* Transfer mode [0..10] */
    uint16_t bytesPerTick;     /* Bytes per tick = (1u << mode) */
    uint8_t i2c_addr;          /* 7-bit I2C address */
    volatile uint8_t busy;     /* Reserved for future use */
} ssd_stream_t;

/**
 * @brief Initialize the SSD1306 display driver
 * 
 * This function configures the display for horizontal addressing mode,
 * sets up the full column/page range, and turns on the display.
 * 
 * @param hi2c Pointer to HAL I2C handle
 * @param i2c_addr 7-bit I2C address (typically 0x3C or 0x3D)
 * @return HAL_StatusTypeDef HAL_OK on success
 */
HAL_StatusTypeDef ssd_init(I2C_HandleTypeDef *hi2c, uint8_t i2c_addr);

/**
 * @brief Set the transfer mode (bytes per tick)
 * 
 * Mode determines how many bytes are sent per tick():
 * - mode 0: 1 byte/tick   (~0.05 ms @ 400kHz I2C)
 * - mode 4: 16 bytes/tick (~0.4 ms)
 * - mode 5: 32 bytes/tick (~0.8 ms) - RECOMMENDED
 * - mode 6: 64 bytes/tick (~1.6 ms)
 * - mode 10: 1024 bytes/tick (~25 ms)
 * 
 * Recommended modes for <1ms execution: 0..5
 * 
 * @param mode Mode value [0..10], clamped internally
 */
void ssd_set_mode(uint8_t mode);

/**
 * @brief Transfer one chunk of framebuffer data
 * 
 * This is the main streaming function. Call it periodically from your
 * main loop or timer callback. It sends bytesPerTick bytes (or remaining
 * bytes until wrap) in a single I2C transaction.
 * 
 * Execution time depends on mode (@ 400kHz I2C):
 * - mode 0-3: <0.5 ms
 * - mode 4-5: 0.5-1.0 ms
 * - mode 6+: >1.0 ms
 * 
 * @note This function blocks for one I2C transaction only
 */
void ssd_tick(void);

/**
 * @brief Get current cursor position (for debugging)
 * 
 * @return Current framebuffer cursor [0..1023]
 */
uint16_t ssd_get_cursor(void);

/**
 * @brief Get pointer to framebuffer
 * 
 * Use this to draw into the framebuffer. The framebuffer layout is:
 * - Horizontal bytes: 0..127 (first row of 8 pixels)
 * - Then bytes 128..255 (second row of 8 pixels), etc.
 * - Total 8 rows of 128 bytes each
 * 
 * Each byte represents 8 vertical pixels (LSB=top, MSB=bottom)
 * 
 * @return Pointer to 1024-byte framebuffer
 */
uint8_t* ssd_get_framebuffer(void);

/**
 * @brief Clear the entire framebuffer
 * 
 * @param pattern Fill pattern (0x00 = all black, 0xFF = all white)
 */
void ssd_clear(uint8_t pattern);

/**
 * @brief Set a single pixel in the framebuffer
 * 
 * @param x X coordinate [0..127]
 * @param y Y coordinate [0..63]
 * @param color 1 = white/on, 0 = black/off
 */
void ssd_set_pixel(uint8_t x, uint8_t y, uint8_t color);

/**
 * @brief Force immediate full framebuffer update (blocking)
 * 
 * This sends the entire framebuffer in one go. Useful for initialization
 * or when you need guaranteed immediate update.
 * 
 * @warning This is blocking and takes ~25ms @ 400kHz I2C
 */
HAL_StatusTypeDef ssd_flush(void);

#endif /* SSD_STREAM_H */
