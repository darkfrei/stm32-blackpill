/**
 * @file    sh1106_conf.h
 * @brief   Configuration file for SH1106 OLED display driver
 * @note    Configure this file for your specific hardware setup
 */

#ifndef __SH1106_CONF_H__
#define __SH1106_CONF_H__

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * MICROCONTROLLER FAMILY SELECTION
 * ======================================================================== */
// Uncomment your MCU family
//#define STM32F0
//#define STM32F1
//#define STM32F3
#define STM32F4
//#define STM32F7
//#define STM32H7
//#define STM32L0
//#define STM32L1
//#define STM32L4
//#define STM32G0
//#define STM32C0
//#define STM32U5

/* ========================================================================
 * BUS SELECTION
 * ======================================================================== */
// Uncomment ONE bus type
#define SH1106_USE_I2C
//#define SH1106_USE_SPI

/* ========================================================================
 * I2C CONFIGURATION
 * ======================================================================== */
#ifdef SH1106_USE_I2C
    extern I2C_HandleTypeDef hi2c1;
    
    #define SH1106_I2C_PORT        hi2c1
    
    // I2C Address: 0x3C or 0x3D (7-bit address, will be shifted automatically)
    // Note: HAL requires 8-bit address (7-bit << 1)
    #define SH1106_I2C_ADDR        (0x3C << 1)  // 0x78 in 8-bit format
    
    // I2C Timeout in milliseconds
    #define SH1106_I2C_TIMEOUT     100
#endif

/* ========================================================================
 * SPI CONFIGURATION
 * ======================================================================== */
#ifdef SH1106_USE_SPI
    extern SPI_HandleTypeDef hspi1;
    
    #define SH1106_SPI_PORT        hspi1
    #define SH1106_CS_Port         GPIOA
    #define SH1106_CS_Pin          GPIO_PIN_4
    #define SH1106_DC_Port         GPIOA
    #define SH1106_DC_Pin          GPIO_PIN_5
    #define SH1106_Reset_Port      GPIOA
    #define SH1106_Reset_Pin       GPIO_PIN_6
#endif

/* ========================================================================
 * DISPLAY CONFIGURATION
 * ======================================================================== */
// Display dimensions
#define SH1106_WIDTH           128
#define SH1106_HEIGHT          64

// Horizontal offset (SH1106 has 132 pixel columns but only 128 are visible)
// Common values: 0 or 2
#define SH1106_X_OFFSET        2

// Display orientation
//#define SH1106_MIRROR_VERT
//#define SH1106_MIRROR_HORIZ

// Invert display colors
//#define SH1106_INVERSE_COLOR

/* ========================================================================
 * PERFORMANCE CONFIGURATION
 * ======================================================================== */

/**
 * Chunk size for incremental screen update (power of 2)
 * 
 * Screen buffer size (128x64): 1024 bytes
 * Number of update calls per frame = 1024 / (2^SH1106_UPDATE_CHUNK_SIZE_POW)
 * 
 * Blocking time per call ≈ (chunk_bytes * 9) / i2c_bitrate
 * where factor 9 = 8 data bits + 1 ack bit
 * 
 * Example for I2C @ 400 kHz:
 * - Full frame: ~23 ms
 * - 128 bytes (2^7): ~2.9 ms/call, 8 calls/frame
 * - 64 bytes (2^6):  ~1.4 ms/call, 16 calls/frame
 * - 32 bytes (2^5):  ~0.7 ms/call, 32 calls/frame
 * 
 * Larger chunks = fewer calls, better throughput, longer blocking
 * Smaller chunks = more calls, better responsiveness, lower throughput
 */
#ifndef SH1106_UPDATE_CHUNK_SIZE_POW
    #define SH1106_UPDATE_CHUNK_SIZE_POW 6  // 64 bytes per call (recommended)
#endif

#define SH1106_UPDATE_CHUNK_SIZE (1 << SH1106_UPDATE_CHUNK_SIZE_POW)

// Enable DMA transfers (requires I2C DMA configuration in CubeMX)
//#define SH1106_USE_DMA

// Enable double buffering (reduces tearing, uses 2x RAM)
//#define SH1106_DOUBLE_BUFFER

/* ========================================================================
 * FONT CONFIGURATION
 * ======================================================================== */
// Uncomment fonts you want to use
#define SH1106_INCLUDE_FONT_6x8
#define SH1106_INCLUDE_FONT_7x10
#define SH1106_INCLUDE_FONT_11x18
#define SH1106_INCLUDE_FONT_8H      // Custom proportional font

/* ========================================================================
 * BUFFER SIZE CALCULATION
 * ======================================================================== */
#define SH1106_BUFFER_SIZE (SH1106_WIDTH * SH1106_HEIGHT / 8)

#ifdef SH1106_DOUBLE_BUFFER
    #define SH1106_TOTAL_BUFFER_SIZE (SH1106_BUFFER_SIZE * 2)
#else
    #define SH1106_TOTAL_BUFFER_SIZE (SH1106_BUFFER_SIZE)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SH1106_CONF_H__ */
