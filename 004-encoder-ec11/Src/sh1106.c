/**
 * @file    sh1106.c
 * @brief   SH1106 OLED Display Driver Implementation
 */

#include "sh1106.h"
#include "sh1106_fonts.h"
#include <string.h>
#include <stdlib.h>

/* ========================================================================
 * PRIVATE DEFINITIONS
 * ======================================================================== */

// SH1106 Commands
#define SH1106_CMD_SET_CONTRAST         0x81
#define SH1106_CMD_DISPLAY_ALL_ON_RAM   0xA4
#define SH1106_CMD_DISPLAY_ALL_ON       0xA5
#define SH1106_CMD_NORMAL_DISPLAY       0xA6
#define SH1106_CMD_INVERSE_DISPLAY      0xA7
#define SH1106_CMD_DISPLAY_OFF          0xAE
#define SH1106_CMD_DISPLAY_ON           0xAF
#define SH1106_CMD_SET_PAGE_ADDR        0xB0
#define SH1106_CMD_SET_COL_ADDR_LOW     0x00
#define SH1106_CMD_SET_COL_ADDR_HIGH    0x10
#define SH1106_CMD_SET_START_LINE       0x40
#define SH1106_CMD_SET_SEGMENT_REMAP_0  0xA0
#define SH1106_CMD_SET_SEGMENT_REMAP_1  0xA1
#define SH1106_CMD_SET_MUX_RATIO        0xA8
#define SH1106_CMD_SET_COM_SCAN_INC     0xC0
#define SH1106_CMD_SET_COM_SCAN_DEC     0xC8
#define SH1106_CMD_SET_DISPLAY_OFFSET   0xD3
#define SH1106_CMD_SET_COM_PINS         0xDA
#define SH1106_CMD_SET_CLOCK_DIV        0xD5
#define SH1106_CMD_SET_PRECHARGE        0xD9
#define SH1106_CMD_SET_VCOM_DESELECT    0xDB
#define SH1106_CMD_SET_PUMP_VOLTAGE     0x30
#define SH1106_CMD_SET_DC_DC            0xAD

/* ========================================================================
 * PRIVATE VARIABLES
 * ======================================================================== */

static uint8_t sh1106_buffer[SH1106_BUFFER_SIZE];

#ifdef SH1106_DOUBLE_BUFFER
static uint8_t sh1106_buffer_back[SH1106_BUFFER_SIZE];
#endif

static SH1106_t sh1106;

/* ========================================================================
 * PRIVATE FUNCTION PROTOTYPES
 * ======================================================================== */

#ifdef SH1106_USE_I2C
static bool SH1106_I2C_WriteCommand(uint8_t cmd);
static bool SH1106_I2C_WriteData(const uint8_t* data, size_t len);
#endif

#ifdef SH1106_USE_SPI
static void SH1106_SPI_WriteCommand(uint8_t cmd);
static void SH1106_SPI_WriteData(const uint8_t* data, size_t len);
#endif

/* ========================================================================
 * LOW-LEVEL I/O FUNCTIONS
 * ======================================================================== */

void SH1106_WriteCommand(uint8_t cmd) {
#ifdef SH1106_USE_I2C
    SH1106_I2C_WriteCommand(cmd);
#elif defined(SH1106_USE_SPI)
    SH1106_SPI_WriteCommand(cmd);
#endif
}

void SH1106_WriteData(const uint8_t* data, size_t len) {
#ifdef SH1106_USE_I2C
    SH1106_I2C_WriteData(data, len);
#elif defined(SH1106_USE_SPI)
    SH1106_SPI_WriteData(data, len);
#endif
}

#ifdef SH1106_USE_I2C
/**
 * @brief Write command via I2C
 */
static bool SH1106_I2C_WriteCommand(uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd};  // 0x00 = command mode
    
#ifdef SH1106_USE_DMA
    if (HAL_I2C_Master_Transmit_DMA(&SH1106_I2C_PORT, SH1106_I2C_ADDR, data, 2) != HAL_OK) {
        return false;
    }
    // Wait for DMA transfer to complete
    while (HAL_I2C_GetState(&SH1106_I2C_PORT) != HAL_I2C_STATE_READY) {}
#else
    if (HAL_I2C_Master_Transmit(&SH1106_I2C_PORT, SH1106_I2C_ADDR, data, 2, SH1106_I2C_TIMEOUT) != HAL_OK) {
        return false;
    }
#endif
    
    return true;
}

/**
 * @brief Write data via I2C
 */
static bool SH1106_I2C_WriteData(const uint8_t* buffer, size_t len) {
    // For I2C, we need to send 0x40 as first byte to indicate data mode
    // We'll send it with the data
    
#ifdef SH1106_USE_DMA
    // For DMA, we need a continuous buffer
    static uint8_t temp_buffer[SH1106_BUFFER_SIZE + 1];
    temp_buffer[0] = 0x40;  // Data mode
    memcpy(&temp_buffer[1], buffer, len);
    
    if (HAL_I2C_Master_Transmit_DMA(&SH1106_I2C_PORT, SH1106_I2C_ADDR, temp_buffer, len + 1) != HAL_OK) {
        return false;
    }
    while (HAL_I2C_GetState(&SH1106_I2C_PORT) != HAL_I2C_STATE_READY) {}
#else
    // For blocking mode, we can use HAL_I2C_Mem_Write which is cleaner
    if (HAL_I2C_Mem_Write(&SH1106_I2C_PORT, SH1106_I2C_ADDR, 0x40, 1, (uint8_t*)buffer, len, SH1106_I2C_TIMEOUT) != HAL_OK) {
        return false;
    }
#endif
    
    return true;
}
#endif

#ifdef SH1106_USE_SPI
/**
 * @brief Write command via SPI
 */
static void SH1106_SPI_WriteCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(SH1106_CS_Port, SH1106_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SH1106_DC_Port, SH1106_DC_Pin, GPIO_PIN_RESET);  // Command mode
    HAL_SPI_Transmit(&SH1106_SPI_PORT, &cmd, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(SH1106_CS_Port, SH1106_CS_Pin, GPIO_PIN_SET);
}

/**
 * @brief Write data via SPI
 */
static void SH1106_SPI_WriteData(const uint8_t* data, size_t len) {
    HAL_GPIO_WritePin(SH1106_CS_Port, SH1106_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SH1106_DC_Port, SH1106_DC_Pin, GPIO_PIN_SET);  // Data mode
    HAL_SPI_Transmit(&SH1106_SPI_PORT, (uint8_t*)data, len, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(SH1106_CS_Port, SH1106_CS_Pin, GPIO_PIN_SET);
}
#endif

/* ========================================================================
 * INITIALIZATION
 * ======================================================================== */

SH1106_Status_t SH1106_Init(void) {
    // Initialize structure
    memset(&sh1106, 0, sizeof(sh1106));
    
    // Small delay after power on
    HAL_Delay(100);
    
#ifdef SH1106_USE_SPI
    // Reset display (SPI mode)
    HAL_GPIO_WritePin(SH1106_Reset_Port, SH1106_Reset_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(SH1106_Reset_Port, SH1106_Reset_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
#endif
    
    // Initialize SH1106 with recommended settings
    SH1106_WriteCommand(SH1106_CMD_DISPLAY_OFF);                // Display OFF
    
    SH1106_WriteCommand(SH1106_CMD_SET_CLOCK_DIV);              // Set clock divider
    SH1106_WriteCommand(0x80);                                  // Default value
    
    SH1106_WriteCommand(SH1106_CMD_SET_MUX_RATIO);              // Set multiplex ratio
    SH1106_WriteCommand(SH1106_HEIGHT - 1);                     // Height - 1
    
    SH1106_WriteCommand(SH1106_CMD_SET_DISPLAY_OFFSET);         // Set display offset
    SH1106_WriteCommand(0x00);                                  // No offset
    
    SH1106_WriteCommand(SH1106_CMD_SET_START_LINE | 0x00);      // Set start line to 0
    
    SH1106_WriteCommand(SH1106_CMD_SET_DC_DC);                  // DC-DC control
    SH1106_WriteCommand(0x8B);                                  // Enable built-in DC-DC
    
    // Screen orientation
#ifdef SH1106_MIRROR_VERT
    SH1106_WriteCommand(SH1106_CMD_SET_COM_SCAN_INC);           // COM scan direction normal
#else
    SH1106_WriteCommand(SH1106_CMD_SET_COM_SCAN_DEC);           // COM scan direction reversed
#endif

#ifdef SH1106_MIRROR_HORIZ
    SH1106_WriteCommand(SH1106_CMD_SET_SEGMENT_REMAP_0);        // Segment remap normal
#else
    SH1106_WriteCommand(SH1106_CMD_SET_SEGMENT_REMAP_1);        // Segment remap reversed
#endif
    
    SH1106_WriteCommand(SH1106_CMD_SET_COM_PINS);               // Set COM pins configuration
    SH1106_WriteCommand(0x12);                                  // Alternative COM pin config
    
    SH1106_WriteCommand(SH1106_CMD_SET_CONTRAST);               // Set contrast
    SH1106_WriteCommand(0xFF);                                  // Maximum contrast
    
    SH1106_WriteCommand(SH1106_CMD_SET_PRECHARGE);              // Set pre-charge period
    SH1106_WriteCommand(0x1F);                                  // Phase 1: 1 DCLK, Phase 2: 15 DCLK
    
    SH1106_WriteCommand(SH1106_CMD_SET_VCOM_DESELECT);          // Set VCOM deselect level
    SH1106_WriteCommand(0x40);                                  // ~0.77 * VCC
    
    SH1106_WriteCommand(SH1106_CMD_DISPLAY_ALL_ON_RAM);         // Display follows RAM content
    
#ifdef SH1106_INVERSE_COLOR
    SH1106_WriteCommand(SH1106_CMD_INVERSE_DISPLAY);            // Inverse display
    sh1106.inverted = true;
#else
    SH1106_WriteCommand(SH1106_CMD_NORMAL_DISPLAY);             // Normal display
    sh1106.inverted = false;
#endif
    
    SH1106_WriteCommand(SH1106_CMD_DISPLAY_ON);                 // Display ON
    
    // Clear screen
    SH1106_Fill(SH1106_COLOR_BLACK);
    SH1106_UpdateScreen();
    
    sh1106.initialized = true;
    
    return SH1106_OK;
}

/* ========================================================================
 * DISPLAY CONTROL
 * ======================================================================== */

void SH1106_ON(void) {
    SH1106_WriteCommand(SH1106_CMD_DISPLAY_ON);
}

void SH1106_OFF(void) {
    SH1106_WriteCommand(SH1106_CMD_DISPLAY_OFF);
}

void SH1106_ToggleInvert(void) {
    sh1106.inverted = !sh1106.inverted;
    if (sh1106.inverted) {
        SH1106_WriteCommand(SH1106_CMD_INVERSE_DISPLAY);
    } else {
        SH1106_WriteCommand(SH1106_CMD_NORMAL_DISPLAY);
    }
}

void SH1106_SetBrightness(uint8_t value) {
    SH1106_WriteCommand(SH1106_CMD_SET_CONTRAST);
    SH1106_WriteCommand(value);
}

/* ========================================================================
 * BUFFER OPERATIONS
 * ======================================================================== */

void SH1106_Fill(SH1106_COLOR_t color) {
    uint8_t fill_value = (color == SH1106_COLOR_BLACK) ? 0x00 : 0xFF;
    memset(sh1106_buffer, fill_value, SH1106_BUFFER_SIZE);
}

void SH1106_Clear(void) {
    SH1106_Fill(SH1106_COLOR_BLACK);
}

uint8_t* SH1106_GetBuffer(void) {
    return sh1106_buffer;
}

void SH1106_UpdateScreen(void) {
    uint8_t page;
    
    for (page = 0; page < SH1106_HEIGHT / 8; page++) {
        // Set page address
        SH1106_WriteCommand(SH1106_CMD_SET_PAGE_ADDR | page);
        
        // Set column address (with offset)
        SH1106_WriteCommand(SH1106_CMD_SET_COL_ADDR_LOW | (SH1106_X_OFFSET & 0x0F));
        SH1106_WriteCommand(SH1106_CMD_SET_COL_ADDR_HIGH | ((SH1106_X_OFFSET >> 4) & 0x0F));
        
        // Write data for this page
        SH1106_WriteData(&sh1106_buffer[SH1106_WIDTH * page], SH1106_WIDTH);
    }
}

bool SH1106_UpdateScreenChunk(uint16_t chunk) {
    uint16_t total_chunks = SH1106_GetTotalChunks();
    
    if (chunk >= total_chunks) {
        return false;  // No more chunks
    }
    
    uint16_t start_byte = chunk * SH1106_UPDATE_CHUNK_SIZE;
    uint16_t bytes_to_send = SH1106_UPDATE_CHUNK_SIZE;
    
    // Check if this is the last chunk
    if (start_byte + bytes_to_send > SH1106_BUFFER_SIZE) {
        bytes_to_send = SH1106_BUFFER_SIZE - start_byte;
    }
    
    // Calculate page and column from byte offset
    uint16_t page = start_byte / SH1106_WIDTH;
    uint16_t col = start_byte % SH1106_WIDTH;
    
    // Set page address
    SH1106_WriteCommand(SH1106_CMD_SET_PAGE_ADDR | page);
    
    // Set column address (with offset)
    uint16_t actual_col = col + SH1106_X_OFFSET;
    SH1106_WriteCommand(SH1106_CMD_SET_COL_ADDR_LOW | (actual_col & 0x0F));
    SH1106_WriteCommand(SH1106_CMD_SET_COL_ADDR_HIGH | ((actual_col >> 4) & 0x0F));
    
    // Write chunk data
    SH1106_WriteData(&sh1106_buffer[start_byte], bytes_to_send);
    
    return (chunk + 1 < total_chunks);  // true if more chunks remain
}

uint16_t SH1106_GetTotalChunks(void) {
    return (SH1106_BUFFER_SIZE + SH1106_UPDATE_CHUNK_SIZE - 1) / SH1106_UPDATE_CHUNK_SIZE;
}

/* ========================================================================
 * DRAWING PRIMITIVES
 * ======================================================================== */

void SH1106_DrawPixel(int16_t x, uint8_t y, SH1106_COLOR_t color) {
    // if (x >= SH1106_WIDTH || y >= SH1106_HEIGHT) {
        // return;  // Out of bounds
    // }
    
    if (x < 0 || x >= SH1106_WIDTH || y >= SH1106_HEIGHT) {
      return;  // Out of bounds
    }
    
    // Calculate buffer position
    // Buffer is organized in pages (8 rows per page)
    uint16_t byte_index = x + (y / 8) * SH1106_WIDTH;
    uint8_t bit_position = y % 8;
    
    if (color == SH1106_COLOR_WHITE) {
        sh1106_buffer[byte_index] |= (1 << bit_position);
    } else {
        sh1106_buffer[byte_index] &= ~(1 << bit_position);
    }
}

void SH1106_DrawLine(int16_t x0, uint8_t y0, int16_t x1, uint8_t y1, SH1106_COLOR_t color) {
    // Bresenham's line algorithm
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    
    while (1) {
        SH1106_DrawPixel(x0, y0, color);
        
        if (x0 == x1 && y0 == y1) {
            break;
        }
        
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

void SH1106_DrawRectangle(int16_t x, uint8_t y, uint8_t w, uint8_t h, SH1106_COLOR_t color) {
    // Draw four lines to form rectangle
    SH1106_DrawLine(x, y, x + w - 1, y, color);              // Top
    SH1106_DrawLine(x, y + h - 1, x + w - 1, y + h - 1, color);  // Bottom
    SH1106_DrawLine(x, y, x, y + h - 1, color);              // Left
    SH1106_DrawLine(x + w - 1, y, x + w - 1, y + h - 1, color);  // Right
}

void SH1106_FillRectangle(int16_t x, uint8_t y, uint8_t w, uint8_t h, SH1106_COLOR_t color) {
    for (uint8_t i = 0; i < h; i++) {
        SH1106_DrawLine(x, y + i, x + w - 1, y + i, color);
    }
}

void SH1106_DrawCircle(int16_t x0, uint8_t y0, uint8_t r, SH1106_COLOR_t color) {
    // Bresenham's circle algorithm
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;
    
    while (x >= y) {
        SH1106_DrawPixel(x0 + x, y0 + y, color);
        SH1106_DrawPixel(x0 + y, y0 + x, color);
        SH1106_DrawPixel(x0 - y, y0 + x, color);
        SH1106_DrawPixel(x0 - x, y0 + y, color);
        SH1106_DrawPixel(x0 - x, y0 - y, color);
        SH1106_DrawPixel(x0 - y, y0 - x, color);
        SH1106_DrawPixel(x0 + y, y0 - x, color);
        SH1106_DrawPixel(x0 + x, y0 - y, color);
        
        y += 1;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x -= 1;
            err += 1 - 2 * x;
        }
    }
}

void SH1106_FillCircle(int16_t x0, uint8_t y0, uint8_t r, SH1106_COLOR_t color) {
    int16_t x = r;
    int16_t y = 0;
    int16_t err = 0;
    
    while (x >= y) {
        SH1106_DrawLine(x0 - x, y0 + y, x0 + x, y0 + y, color);
        SH1106_DrawLine(x0 - y, y0 + x, x0 + y, y0 + x, color);
        SH1106_DrawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
        SH1106_DrawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);
        
        y += 1;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x -= 1;
            err += 1 - 2 * x;
        }
    }
}

void SH1106_DrawBitmap(int16_t x, uint8_t y, const uint8_t* bitmap, uint8_t w, uint8_t h, SH1106_COLOR_t color) {
    for (uint8_t j = 0; j < h; j++) {
        for (uint8_t i = 0; i < w; i++) {
            uint8_t byte = bitmap[j * ((w + 7) / 8) + i / 8];
            uint8_t bit = byte & (1 << (7 - (i % 8)));
            
            if (bit) {
                SH1106_DrawPixel(x + i, y + j, color);
            }
        }
    }
}

/* ========================================================================
 * TEXT RENDERING (pixel-clipped, supports negative X)
 * ======================================================================== */

void SH1106_SetCursor(int16_t x, uint8_t y) {
    sh1106.current_x = x;
    sh1106.current_y = y;
}

void SH1106_GetCursor(int16_t* x, uint16_t* y) {
    if (x) {
        *x = sh1106.current_x;
    }
    if (y) {
        *y = sh1106.current_y;
    }
}

char SH1106_WriteChar(char ch, SH1106_Font_t font, SH1106_COLOR_t color) {
    if (ch < 32 || ch > 126) {
        return 0;
    }

    uint8_t char_index = (uint8_t)(ch - 32);
    uint8_t char_width =
        font.char_width ? font.char_width[char_index] : font.width;

    int8_t y_offset = font.y_offset ? font.y_offset[char_index] : 0;

    int16_t base_x = sh1106.current_x;
    int16_t base_y = (int16_t)sh1106.current_y
                   + font.baseline
                   + y_offset;

    uint16_t glyph_offset = (uint16_t)char_index * font.height;

    /* determine visible column range in glyph */
    int16_t glyph_col_start = 0;
    int16_t glyph_col_end   = char_width;

    if (base_x < 0) {
        glyph_col_start = -base_x;
    }

    if (base_x + char_width > SH1106_WIDTH) {
        glyph_col_end = SH1106_WIDTH - base_x;
    }

    if (glyph_col_start >= glyph_col_end) {
        sh1106.current_x += char_width + 1;
        return ch;
    }

    for (uint8_t row = 0; row < font.height; row++) {
        uint16_t row_bits = font.data[glyph_offset + row];

        for (int16_t glyph_col = glyph_col_start;
             glyph_col < glyph_col_end;
             glyph_col++) {

            if (row_bits & (1U << (15 - glyph_col))) {

                int16_t px = base_x + glyph_col;
                int16_t py = base_y + row - font.baseline;

                if (py >= 0 && py < SH1106_HEIGHT) {
                    SH1106_DrawPixel(
                        (uint8_t)px,
                        (uint8_t)py,
                        color
                    );
                }
            }
        }
    }

    // sh1106.current_x += char_width + 1;
    sh1106.current_x += char_width;
    return ch;
}

uint16_t SH1106_WriteString(const char* str,
                            SH1106_Font_t font,
                            SH1106_COLOR_t color) {
    uint16_t written = 0;

    while (*str) {
        if (SH1106_WriteChar(*str, font, color)) {
            written++;
        }
        str++;
    }

    return written;
}

uint16_t SH1106_WriteStringAt(int16_t x,
                              uint8_t y,
                              const char* str,
                              SH1106_Font_t font,
                              SH1106_COLOR_t color) {
    SH1106_SetCursor(x, y);
    return SH1106_WriteString(str, font, color);
}

uint16_t SH1106_GetStringWidth(const char* str, SH1106_Font_t font) {
    uint16_t width = 0;

    while (*str) {
        if (*str >= 32 && *str <= 126) {
            if (font.char_width) {
                width += font.char_width[*str - 32] + 1;
            } else {
                width += font.width + 1;
            }
        }
        str++;
    }

    /* remove last spacing */
    return (width > 0) ? (width - 1) : 0;
}
