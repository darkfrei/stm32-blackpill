/**
 * Private configuration file for the SSD1306 library.
 * This example is configured for STM32F0, I2C and including all fonts.
 */

// start of added code
 
/* Chunk size for incremental screen update (power of 2) */
/* 2^6 = 64 bytes, 2^7 = 128 bytes, etc. */
/* chunk size for incremental screen update
 * value is a power of two: 2^n bytes per transfer
 *
 * screen size (ssd1306 128x64):
 * 128 * 64 / 8 = 1024 bytes total framebuffer
 *
 * number of update calls per full frame:
 * calls = 1024 / (2^SSD1306_UPDATE_CHUNK_SIZE_POW)
 *
 * approximate blocking time per update call:
 * t_block_call ~= (chunk_bytes * 9) / i2c_bitrate
 *
 * approximate blocking time per full screen refresh:
 * t_block_frame ~= (1024 * 9) / i2c_bitrate
 *
 * where:
 *  - chunk_bytes = 2^SSD1306_UPDATE_CHUNK_SIZE_POW
 *  - factor 9 accounts for 8 data bits + 1 ack bit
 *  - i2c_bitrate in bits per second (e.g. 400000)
 *
 * example for i2c = 400 kbit/s:
 *
 * full frame blocking time:
 * t_block_frame ~= (1024 * 9) / 400000 ~= 23 ms
 *
 * per call blocking time:
 * 2^7 = 128 bytes  -> ~2.9 ms per call, 8 calls per frame
 * 2^6 =  64 bytes  -> ~1.4 ms per call, 16 calls per frame
 * 2^5 =  32 bytes  -> ~0.7 ms per call, 32 calls per frame
 *
 * larger chunks:
 *  - fewer update calls
 *  - longer blocking time per call
 *  - better throughput
 *
 * smaller chunks:
 *  - more update calls
 *  - shorter blocking time per call
 *  - better main loop responsiveness
 */
#ifndef SSD1306_UPDATE_CHUNK_SIZE_POW

/* UPS is correct for the example only */

/* uncomment exactly one value */

// #define SSD1306_UPDATE_CHUNK_SIZE_POW 10  /* 1024 bytes per update call, UPS: 240 */
// #define SSD1306_UPDATE_CHUNK_SIZE_POW 9   /*  512 bytes per update call, UPS: 240 */
// #define SSD1306_UPDATE_CHUNK_SIZE_POW 8   /*  256 bytes per update call, UPS: 240 */
// #define SSD1306_UPDATE_CHUNK_SIZE_POW 7   /*  128 bytes per update call, UPS: 240 (yes, same) */
// #define SSD1306_UPDATE_CHUNK_SIZE_POW 6   /*   64 bytes per update call, UPS: 430  */
// #define SSD1306_UPDATE_CHUNK_SIZE_POW 5   /*   32 bytes per update call, UPS: 712  */
#define SSD1306_UPDATE_CHUNK_SIZE_POW 4   /*   16 bytes per update call, UPS: 1062 */
// #define SSD1306_UPDATE_CHUNK_SIZE_POW 3   /*    8 bytes per update call, UPS: 1406 */
// #define SSD1306_UPDATE_CHUNK_SIZE_POW 2   /*    4 bytes per update call, UPS: 1682 */

 /* LOWE THAN SSD1306_UPDATE_CHUNK_SIZE_POW 2   -    4 bytes per update call DOESNT WORK */
 /* LOWE THAN SSD1306_UPDATE_CHUNK_SIZE_POW 2   -    4 bytes per update call DOESNT WORK */
 /* LOWE THAN SSD1306_UPDATE_CHUNK_SIZE_POW 2   -    4 bytes per update call DOESNT WORK */

#endif


#define SSD1306_UPDATE_CHUNK_SIZE (1 << SSD1306_UPDATE_CHUNK_SIZE_POW)

// end of added code

#ifndef __SSD1306_CONF_H__
#define __SSD1306_CONF_H__

// Choose a microcontroller family
//#define STM32F0
//#define STM32F1
#define STM32F4
//#define STM32L0
//#define STM32L1
//#define STM32L4
//#define STM32F3
//#define STM32H7
//#define STM32F7
//#define STM32G0
//#define STM32C0
//#define STM32U5

// Choose a bus
#define SSD1306_USE_I2C
//#define SSD1306_USE_SPI

// I2C Configuration
#define SSD1306_I2C_PORT        hi2c1
#define SSD1306_I2C_ADDR        (0x3C << 1)

// SPI Configuration
//#define SSD1306_SPI_PORT        hspi1
//#define SSD1306_CS_Port         OLED_CS_GPIO_Port
//#define SSD1306_CS_Pin          OLED_CS_Pin
//#define SSD1306_DC_Port         OLED_DC_GPIO_Port
//#define SSD1306_DC_Pin          OLED_DC_Pin
//#define SSD1306_Reset_Port      OLED_Res_GPIO_Port
//#define SSD1306_Reset_Pin       OLED_Res_Pin

// Mirror the screen if needed
// #define SSD1306_MIRROR_VERT
// #define SSD1306_MIRROR_HORIZ

// Set inverse color if needed
// # define SSD1306_INVERSE_COLOR

// Include only needed fonts

#define SSD1306_INCLUDE_FONT_6x8
#define SSD1306_INCLUDE_FONT_7x10
#define SSD1306_INCLUDE_FONT_11x18

// The width of the screen can be set using this
// define. The default value is 128.
// #define SSD1306_WIDTH           64

// If your screen horizontal axis does not start
// in column 0 you can use this define to
// adjust the horizontal offset
#define SSD1306_X_OFFSET 2

// The height can be changed as well if necessary.
// It can be 32, 64 or 128. The default value is 64.
// #define SSD1306_HEIGHT          64

#endif /* __SSD1306_CONF_H__ */
