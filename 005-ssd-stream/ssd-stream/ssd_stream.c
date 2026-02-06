/**
 * @file ssd_stream.c
 * @brief Non-blocking streaming driver for SSD1306-compatible OLED displays
 * 
 * @author Claude (Anthropic AI)
 * @date 2026-01-31
 */

#include "ssd_stream.h"
#include <string.h>

/* SSD1306 command definitions */
#define SSD_CMD_SET_CONTRAST        0x81
#define SSD_CMD_DISPLAY_ALL_ON_RESUME 0xA4
#define SSD_CMD_DISPLAY_NORMAL      0xA6
#define SSD_CMD_DISPLAY_OFF         0xAE
#define SSD_CMD_DISPLAY_ON          0xAF
#define SSD_CMD_SET_DISPLAY_OFFSET  0xD3
#define SSD_CMD_SET_COMPINS         0xDA
#define SSD_CMD_SET_VCOMDETECT      0xDB
#define SSD_CMD_SET_DISPLAY_CLK_DIV 0xD5
#define SSD_CMD_SET_PRECHARGE       0xD9
#define SSD_CMD_SET_MULTIPLEX       0xA8
#define SSD_CMD_SET_LOW_COLUMN      0x00
#define SSD_CMD_SET_HIGH_COLUMN     0x10
#define SSD_CMD_SET_START_LINE      0x40
#define SSD_CMD_MEMORY_MODE         0x20
#define SSD_CMD_COLUMN_ADDR         0x21
#define SSD_CMD_PAGE_ADDR           0x22
#define SSD_CMD_COM_SCAN_DEC        0xC8
#define SSD_CMD_SEG_REMAP           0xA1
#define SSD_CMD_CHARGE_PUMP         0x8D

/* Memory addressing modes */
#define SSD_MEM_MODE_HORIZONTAL     0x00
#define SSD_MEM_MODE_VERTICAL       0x01
#define SSD_MEM_MODE_PAGE           0x02

/* Global driver state */
static ssd_stream_t g_ssd_ctx;

/* Framebuffer (1024 bytes = 128 * 64 / 8) */
static uint8_t g_ssd_fb[SSD_FB_SIZE];

/* Static buffer for I2C transmission (1 control byte + max chunk) */
static uint8_t g_tx_buffer[1 + 1024];

/**
 * @brief Send command bytes to display
 * 
 * @param cmds Pointer to command bytes
 * @param len Number of command bytes
 * @return HAL_StatusTypeDef
 */
static HAL_StatusTypeDef ssd_write_cmds(const uint8_t *cmds, uint8_t len)
{
    HAL_StatusTypeDef status;
    
    if (len == 0 || len > 127) {
        return HAL_ERROR;
    }
    
    /* Prepare buffer: control byte + commands */
    g_tx_buffer[0] = SSD_CTRL_CMD;
    memcpy(&g_tx_buffer[1], cmds, len);
    
    /* Transmit */
    status = HAL_I2C_Master_Transmit(
        g_ssd_ctx.hi2c,
        (g_ssd_ctx.i2c_addr << 1),
        g_tx_buffer,
        len + 1,
        SSD_I2C_TIMEOUT
    );
    
    return status;
}

/**
 * @brief Initialize display with standard SSD1306 sequence
 */
HAL_StatusTypeDef ssd_init(I2C_HandleTypeDef *hi2c, uint8_t i2c_addr)
{
    HAL_StatusTypeDef status;
    
    /* Initialize context */
    memset(&g_ssd_ctx, 0, sizeof(g_ssd_ctx));
    g_ssd_ctx.hi2c = hi2c;
    g_ssd_ctx.i2c_addr = i2c_addr;
    g_ssd_ctx.cursor = 0;
    g_ssd_ctx.mode = 4;  /* Default: 16 bytes per tick */
    g_ssd_ctx.bytesPerTick = 1u << g_ssd_ctx.mode;
    g_ssd_ctx.busy = 0;
    
    /* Clear framebuffer */
    memset(g_ssd_fb, 0x00, SSD_FB_SIZE);
    
    /* Small delay for display power-up */
    HAL_Delay(100);
    
    /* Initialization command sequence for SSD1306 */
    const uint8_t init_seq[] = {
        SSD_CMD_DISPLAY_OFF,                    /* Display OFF */
        
        SSD_CMD_SET_DISPLAY_CLK_DIV, 0x80,      /* Set clock divide/oscillator */
        SSD_CMD_SET_MULTIPLEX, 0x3F,            /* Multiplex ratio (64-1) */
        SSD_CMD_SET_DISPLAY_OFFSET, 0x00,       /* Display offset = 0 */
        SSD_CMD_SET_START_LINE | 0x00,          /* Start line = 0 */
        
        SSD_CMD_CHARGE_PUMP, 0x14,              /* Enable charge pump (0x14=on, 0x10=off) */
        
        /* CRITICAL: Set horizontal addressing mode */
        SSD_CMD_MEMORY_MODE, SSD_MEM_MODE_HORIZONTAL,
        
        SSD_CMD_SEG_REMAP,                      /* Segment remap (A0/A1) */
        SSD_CMD_COM_SCAN_DEC,                   /* COM scan direction */
        
        SSD_CMD_SET_COMPINS, 0x12,              /* COM pins config (alt, disable remap) */
        SSD_CMD_SET_CONTRAST, 0x7F,             /* Contrast (0x00-0xFF) */
        SSD_CMD_SET_PRECHARGE, 0xF1,            /* Pre-charge period */
        SSD_CMD_SET_VCOMDETECT, 0x40,           /* VCOM deselect level */
        
        SSD_CMD_DISPLAY_ALL_ON_RESUME,          /* Resume from RAM */
        SSD_CMD_DISPLAY_NORMAL,                 /* Normal (non-inverted) */
        
        /* Set full column/page address range for horizontal mode */
        SSD_CMD_COLUMN_ADDR, 0x00, 0x7F,        /* Column 0..127 */
        SSD_CMD_PAGE_ADDR, 0x00, 0x07,          /* Page 0..7 */
        
        SSD_CMD_DISPLAY_ON                      /* Display ON */
    };
    
    status = ssd_write_cmds(init_seq, sizeof(init_seq));
    
    if (status != HAL_OK) {
        return status;
    }
    
    /* Small delay before first write */
    HAL_Delay(10);
    
    return HAL_OK;
}

void ssd_set_mode(uint8_t mode)
{
    /* Clamp mode to valid range */
    if (mode > SSD_MODE_MAX) {
        mode = SSD_MODE_MAX;
    }
    
    g_ssd_ctx.mode = mode;
    g_ssd_ctx.bytesPerTick = 1u << mode;  /* MUST use unsigned shift */
}

void ssd_tick(void)
{
    uint16_t remaining;
    uint16_t chunk;
    HAL_StatusTypeDef status;
    
    /* Calculate remaining bytes to end of buffer */
    remaining = SSD_FB_SIZE - g_ssd_ctx.cursor;
    
    /* Determine chunk size (min of bytesPerTick and remaining) */
    chunk = g_ssd_ctx.bytesPerTick;
    if (chunk > remaining) {
        chunk = remaining;
    }
    
    /* Prepare transmission buffer: control byte + data */
    g_tx_buffer[0] = SSD_CTRL_DATA;
    memcpy(&g_tx_buffer[1], &g_ssd_fb[g_ssd_ctx.cursor], chunk);
    
    /* Transmit data chunk (blocking, but short) */
    status = HAL_I2C_Master_Transmit(
        g_ssd_ctx.hi2c,
        (g_ssd_ctx.i2c_addr << 1),
        g_tx_buffer,
        chunk + 1,  /* +1 for control byte */
        SSD_I2C_TIMEOUT
    );
    
    /* Update cursor (wrap around if needed) */
    if (status == HAL_OK) {
        g_ssd_ctx.cursor += chunk;
        if (g_ssd_ctx.cursor >= SSD_FB_SIZE) {
            g_ssd_ctx.cursor = 0;
        }
    }
    /* Note: On error, cursor doesn't advance - will retry next tick */
}

uint16_t ssd_get_cursor(void)
{
    return g_ssd_ctx.cursor;
}

uint8_t* ssd_get_framebuffer(void)
{
    return g_ssd_fb;
}

void ssd_clear(uint8_t pattern)
{
    memset(g_ssd_fb, pattern, SSD_FB_SIZE);
}

void ssd_set_pixel(uint8_t x, uint8_t y, uint8_t color)
{
    uint16_t byte_index;
    uint8_t bit_mask;
    
    /* Bounds check */
    if (x >= SSD_WIDTH || y >= SSD_HEIGHT) {
        return;
    }
    
    /* Calculate byte index in framebuffer
     * Layout: 128 bytes per page, 8 pages total
     * Each byte controls 8 vertical pixels (LSB=top, MSB=bottom)
     */
    byte_index = (y / 8) * SSD_WIDTH + x;
    bit_mask = 1 << (y % 8);
    
    if (color) {
        g_ssd_fb[byte_index] |= bit_mask;   /* Set pixel */
    } else {
        g_ssd_fb[byte_index] &= ~bit_mask;  /* Clear pixel */
    }
}

HAL_StatusTypeDef ssd_flush(void)
{
    HAL_StatusTypeDef status;
    
    /* Prepare full framebuffer with control byte */
    g_tx_buffer[0] = SSD_CTRL_DATA;
    memcpy(&g_tx_buffer[1], g_ssd_fb, SSD_FB_SIZE);
    
    /* Send entire framebuffer in one transaction */
    status = HAL_I2C_Master_Transmit(
        g_ssd_ctx.hi2c,
        (g_ssd_ctx.i2c_addr << 1),
        g_tx_buffer,
        SSD_FB_SIZE + 1,
        100  /* Longer timeout for full transfer */
    );
    
    /* Reset cursor after flush */
    if (status == HAL_OK) {
        g_ssd_ctx.cursor = 0;
    }
    
    return status;
}
