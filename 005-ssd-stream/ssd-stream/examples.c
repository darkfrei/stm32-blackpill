/**
 * @file examples.c
 * @brief Usage examples and test patterns for SSD1306 streaming driver
 * 
 * This file contains various examples showing how to use the driver
 * in different scenarios and test patterns for visual verification.
 * 
 * @author Claude (Anthropic AI)
 * @date 2026-01-31
 */

#include "ssd_stream.h"
#include "ssd_utils.h"
#include "main.h"

/* External I2C handle (defined in main.c) */
extern I2C_HandleTypeDef hi2c1;

/* ================================================================
 * Test Pattern Functions
 * ================================================================ */

/**
 * @brief Fill framebuffer with checkerboard pattern
 */
void test_pattern_checkerboard(void)
{
    uint8_t *fb = ssd_get_framebuffer();
    
    for (uint16_t i = 0; i < SSD_FB_SIZE; i++) {
        /* Alternating pattern every 8 pixels */
        if ((i / 8) % 2) {
            fb[i] = 0xAA;  /* 10101010 */
        } else {
            fb[i] = 0x55;  /* 01010101 */
        }
    }
}

/**
 * @brief Draw horizontal stripes
 */
void test_pattern_stripes_h(void)
{
    uint8_t *fb = ssd_get_framebuffer();
    
    /* Each page (8 rows) alternates */
    for (uint8_t page = 0; page < 8; page++) {
        uint8_t pattern = (page % 2) ? 0xFF : 0x00;
        memset(&fb[page * SSD_WIDTH], pattern, SSD_WIDTH);
    }
}

/**
 * @brief Draw vertical stripes
 */
void test_pattern_stripes_v(void)
{
    ssd_clear(0x00);
    
    /* Draw vertical lines every 8 pixels */
    for (uint8_t x = 0; x < SSD_WIDTH; x += 8) {
        for (uint8_t y = 0; y < SSD_HEIGHT; y++) {
            ssd_set_pixel(x, y, 1);
        }
    }
}

/**
 * @brief Draw a border around the display
 */
void test_pattern_border(void)
{
    ssd_clear(0x00);
    
    /* Top and bottom borders */
    for (uint8_t x = 0; x < SSD_WIDTH; x++) {
        ssd_set_pixel(x, 0, 1);
        ssd_set_pixel(x, SSD_HEIGHT - 1, 1);
    }
    
    /* Left and right borders */
    for (uint8_t y = 0; y < SSD_HEIGHT; y++) {
        ssd_set_pixel(0, y, 1);
        ssd_set_pixel(SSD_WIDTH - 1, y, 1);
    }
}

/**
 * @brief Animated walking pixel pattern
 */
void test_pattern_walking_pixel(uint8_t offset)
{
    ssd_clear(0x00);
    
    /* Single pixel moving in a line */
    uint16_t pos = offset % SSD_FB_SIZE;
    uint8_t page = pos / SSD_WIDTH;
    uint8_t col = pos % SSD_WIDTH;
    uint8_t row = page * 8;
    
    ssd_set_pixel(col, row, 1);
}

/**
 * @brief Gradient pattern (increasing density from left to right)
 */
void test_pattern_gradient(void)
{
    uint8_t *fb = ssd_get_framebuffer();
    
    for (uint8_t page = 0; page < 8; page++) {
        for (uint8_t col = 0; col < SSD_WIDTH; col++) {
            /* Create density gradient */
            uint8_t density = col / 16;  /* 0..7 */
            uint8_t pattern;
            
            switch (density) {
                case 0: pattern = 0x00; break;
                case 1: pattern = 0x11; break;  /* Sparse dots */
                case 2: pattern = 0x22; break;
                case 3: pattern = 0x44; break;
                case 4: pattern = 0x55; break;
                case 5: pattern = 0xAA; break;
                case 6: pattern = 0xDD; break;
                default: pattern = 0xFF; break;  /* Solid */
            }
            
            fb[page * SSD_WIDTH + col] = pattern;
        }
    }
}

/* ================================================================
 * Example Usage Scenarios
 * ================================================================ */

/**
 * @brief Basic initialization example
 * 
 * Add this to your main() function after HAL initialization
 */
void example_basic_init(void)
{
    HAL_StatusTypeDef status;
    
    /* I2C should already be initialized via CubeMX */
    /* Common I2C addresses: 0x3C or 0x3D */
    
    status = ssd_init(&hi2c1, 0x3C);
    
    if (status != HAL_OK) {
        /* Handle initialization error */
        Error_Handler();
    }
    
    /* Set transfer mode (0..10) */
    ssd_set_mode(5);  /* 32 bytes/tick ~= 0.8ms @ 400kHz */
    
    /* Draw test pattern */
    test_pattern_border();
    
    /* Optional: Force immediate update */
    ssd_flush();
}

/**
 * @brief Main loop with streaming update
 * 
 * Call ssd_tick() periodically from your main loop
 */
void example_main_loop(void)
{
    uint32_t frame_counter = 0;
    
    while (1) {
        /* Regular tick - streams framebuffer to display */
        ssd_tick();
        
        /* Update animation every 1000 ticks */
        if (++frame_counter >= 1000) {
            frame_counter = 0;
            /* Update framebuffer with new content */
            test_pattern_walking_pixel((HAL_GetTick() / 10) & 0xFF);
        }
        
        /* Other application tasks */
        HAL_Delay(1);  /* Or use your own scheduling */
    }
}

/**
 * @brief Timer-based streaming (using SysTick or hardware timer)
 * 
 * Call ssd_tick() from a timer interrupt for precise timing
 */
void example_timer_callback(void)
{
    /* Called every 1ms from timer ISR */
    static uint8_t tick_divider = 0;
    
    if (++tick_divider >= 2) {  /* Every 2ms */
        tick_divider = 0;
        ssd_tick();
    }
}

/**
 * @brief Full demonstration sequence
 * 
 * Cycles through different test patterns
 */
void example_demo_sequence(void)
{
    uint8_t pattern = 0;
    uint32_t last_change = HAL_GetTick();
    
    while (1) {
        /* Change pattern every 3 seconds */
        if (HAL_GetTick() - last_change > 3000) {
            last_change = HAL_GetTick();
            
            switch (pattern) {
                case 0: test_pattern_border(); break;
                case 1: test_pattern_checkerboard(); break;
                case 2: test_pattern_stripes_h(); break;
                case 3: test_pattern_stripes_v(); break;
                case 4: test_pattern_gradient(); break;
            }
            
            pattern = (pattern + 1) % 5;
        }
        
        /* Stream update */
        ssd_tick();
        
        HAL_Delay(1);
    }
}

/**
 * @brief Graphics utilities example (requires ssd_utils.h)
 */
void example_graphics(void)
{
    ssd_clear(0x00);
    
    /* Draw some shapes */
    ssd_draw_rect(10, 10, 40, 20, 1);
    ssd_fill_circle(80, 32, 15, 1);
    ssd_draw_line(0, 0, 127, 63, 1);
    
    /* Draw text (if ssd_utils included) */
    ssd_draw_string(20, 50, "HELLO", NULL, 1, 1);
    
    ssd_flush();
}

/* ================================================================
 * Minimal main.c Template
 * ================================================================ */

#if 0  /* Template code - don't compile, just for reference */

int main(void)
{
    /* HAL initialization */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();  /* Configure I2C1 @ 400kHz in CubeMX */
    
    /* Initialize display driver */
    if (ssd_init(&hi2c1, 0x3C) != HAL_OK) {
        Error_Handler();
    }
    
    /* Set mode: 5 = 32 bytes/tick ~= 0.8ms */
    ssd_set_mode(5);
    
    /* Draw initial pattern */
    test_pattern_border();
    
    /* Flush immediately to see the pattern */
    ssd_flush();
    
    /* Main loop */
    while (1) {
        /* Stream framebuffer to display */
        ssd_tick();
        
        /* Your application code here */
        
        HAL_Delay(1);
    }
}

#endif
