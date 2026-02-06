# Integration Guide - SSD1306 Streaming Driver

## Quick Start (5 minutes)

### Step 1: STM32CubeMX Setup

1. Open STM32CubeMX
2. Select MCU: STM32F411CEU6 (BlackPill)
3. Configure I2C1:
   - Mode: I2C
   - I2C Speed Mode: Fast Mode
   - I2C Clock Speed: 400000 Hz
   - GPIO Settings:
     * SCL: PB6 (or your pin)
     * SDA: PB7 (or your pin)
     * Pull-up: Internal pull-up (or external 4.7kΩ)
     * Speed: High

4. System Core:
   - SYS → Debug: Serial Wire
   - RCC → HSE: Crystal/Ceramic Resonator (if using external crystal)

5. Clock Configuration:
   - HCLK: 100 MHz (maximum for F411)

6. Project Manager:
   - Toolchain: Makefile / STM32CubeIDE / Keil
   - Generate code

### Step 2: Copy Driver Files

```bash
# In your project folder
cd YourProject

# Create structure
mkdir -p Drivers/SSD1306

# Copy files
cp ssd_stream.h Core/Inc/
cp ssd_stream.c Core/Src/
cp ssd_stream_example.c Core/Src/  # optional
cp ssd_stream_test.c Core/Src/     # optional
```

### Step 3: Modify main.c

Open `Core/Src/main.c` and add:

```c
/* USER CODE BEGIN Includes */
#include "ssd_stream.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 2 */
// After HAL_Init() and peripheral initialization

// Initialize display
if (ssd_init(&hi2c1, 0x3C) != HAL_OK) {
    Error_Handler();
}

// Set mode
ssd_set_mode(5);  // 32 bytes/tick ≈ 0.8 ms

// Test pattern
ssd_clear(0x00);

// Draw border
for (uint8_t x = 0; x < 128; x++) {
    ssd_set_pixel(x, 0, 1);
    ssd_set_pixel(x, 63, 1);
}
for (uint8_t y = 0; y < 64; y++) {
    ssd_set_pixel(0, y, 1);
    ssd_set_pixel(127, y, 1);
}

// Immediate display
ssd_flush();

/* USER CODE END 2 */

/* USER CODE BEGIN WHILE */
while (1)
{
    // Streaming update
    ssd_tick();
    
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    
    HAL_Delay(1);
}
/* USER CODE END 3 */
```

### Step 4: Build and Flash

```bash
# For Makefile project
make clean
make -j4

# Flash via ST-Link
st-flash write build/YourProject.bin 0x8000000

# Or via OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build/YourProject.elf verify reset exit"
```

## Real-World Example: System Monitor

```c
/**
 * @file system_monitor.c
 * @brief Real-world example: System monitor with OLED display
 */

#include "ssd_stream.h"
#include "main.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c1;

// Simple 5x7 font for digits (example only)
static const uint8_t font_5x7[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
};

/**
 * @brief Draw a character using 5x7 font
 */
void draw_char(uint8_t x, uint8_t y, char c)
{
    if (c < '0' || c > '9') return;
    
    uint8_t digit = c - '0';
    
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t column_data = font_5x7[digit][col];
        
        for (uint8_t bit = 0; bit < 7; bit++) {
            if (column_data & (1 << bit)) {
                ssd_set_pixel(x + col, y + bit, 1);
            }
        }
    }
}

/**
 * @brief Draw a string of digits
 */
void draw_string(uint8_t x, uint8_t y, const char *str)
{
    uint8_t pos = x;
    while (*str) {
        draw_char(pos, y, *str);
        pos += 6;  // 5 pixels + 1 spacing
        str++;
    }
}

/**
 * @brief Draw a progress bar
 */
void draw_progress_bar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t percent)
{
    if (percent > 100) percent = 100;
    
    // Border
    for (uint8_t i = 0; i < width; i++) {
        ssd_set_pixel(x + i, y, 1);
        ssd_set_pixel(x + i, y + height - 1, 1);
    }
    for (uint8_t i = 0; i < height; i++) {
        ssd_set_pixel(x, y + i, 1);
        ssd_set_pixel(x + width - 1, y + i, 1);
    }
    
    // Fill
    uint8_t fill_width = ((width - 2) * percent) / 100;
    for (uint8_t j = 1; j < height - 1; j++) {
        for (uint8_t i = 1; i < fill_width + 1; i++) {
            ssd_set_pixel(x + i, y + j, 1);
        }
    }
}

/**
 * @brief Main system monitor application
 */
void system_monitor_init(void)
{
    // Initialize display
    if (ssd_init(&hi2c1, 0x3C) != HAL_OK) {
        Error_Handler();
    }
    
    ssd_set_mode(5);  // 32 bytes/tick
    ssd_clear(0x00);
    ssd_flush();
}

void system_monitor_update(void)
{
    static uint32_t last_update = 0;
    static uint8_t cpu_load = 0;
    
    // Update display every 500ms
    if (HAL_GetTick() - last_update < 500) {
        ssd_tick();  // Keep streaming
        return;
    }
    
    last_update = HAL_GetTick();
    
    // Clear screen
    ssd_clear(0x00);
    
    // CPU load progress bar
    draw_progress_bar(10, 10, 108, 8, cpu_load);
    
    // CPU load percentage
    char buf[8];
    snprintf(buf, sizeof(buf), "%3d", cpu_load);
    draw_string(10, 25, buf);
    
    // Uptime
    uint32_t uptime_sec = HAL_GetTick() / 1000;
    uint32_t minutes = uptime_sec / 60;
    snprintf(buf, sizeof(buf), "%04lu", minutes);
    draw_string(10, 45, buf);
    
    // Simulate varying CPU load
    cpu_load = (cpu_load + 5) % 100;
}

void system_monitor_main(void)
{
    system_monitor_init();
    
    while (1) {
        system_monitor_update();
        HAL_Delay(1);
    }
}
```

## FreeRTOS Integration

```c
/**
 * @file freertos_integration.c
 * @brief Example of using SSD driver with FreeRTOS
 */

#include "FreeRTOS.h"
#include "task.h"
#include "ssd_stream.h"

/* Task handles */
TaskHandle_t displayTaskHandle;
TaskHandle_t appTaskHandle;

/**
 * @brief Display streaming task (high priority)
 */
void DisplayTask(void *argument)
{
    // Initialize display
    ssd_init(&hi2c1, 0x3C);
    ssd_set_mode(5);
    ssd_clear(0x00);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1);  // 1ms
    
    for(;;)
    {
        // Stream one chunk
        ssd_tick();
        
        // Wait for next cycle (1ms period)
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Application task (draws content)
 */
void AppTask(void *argument)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100);  // 100ms
    
    uint8_t counter = 0;
    
    for(;;)
    {
        // Update framebuffer (thread-safe if only this task writes)
        ssd_clear(0x00);
        
        // Draw some content
        for (uint8_t i = 0; i < counter; i++) {
            ssd_set_pixel(i, 32, 1);
        }
        
        counter = (counter + 1) % 128;
        
        // Wait for next update
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Create FreeRTOS tasks
 */
void create_display_tasks(void)
{
    // High priority for streaming task
    xTaskCreate(DisplayTask, "Display", 256, NULL, 
                osPriorityHigh, &displayTaskHandle);
    
    // Normal priority for app task
    xTaskCreate(AppTask, "App", 512, NULL, 
                osPriorityNormal, &appTaskHandle);
}
```

## Timer-Based Streaming

```c
/**
 * @file timer_integration.c
 * @brief Example using hardware timer for precise streaming
 */

#include "ssd_stream.h"
#include "main.h"

extern TIM_HandleTypeDef htim2;

/**
 * @brief Timer callback (called every 1ms)
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        // Stream one chunk every timer tick
        ssd_tick();
    }
}

/**
 * @brief Setup timer-based streaming
 */
void setup_timer_streaming(void)
{
    // Initialize display
    ssd_init(&hi2c1, 0x3C);
    ssd_set_mode(4);  // 16 bytes/tick for low latency
    
    // Start timer (1kHz, configured in CubeMX)
    HAL_TIM_Base_Start_IT(&htim2);
}
```

## Performance Profiling

### Using GPIO for Profiling

```c
/**
 * @brief Profile tick() execution time using GPIO
 */

#define PROFILE_PIN GPIO_PIN_13
#define PROFILE_PORT GPIOC

void profile_tick(void)
{
    HAL_GPIO_WritePin(PROFILE_PORT, PROFILE_PIN, GPIO_PIN_SET);
    ssd_tick();
    HAL_GPIO_WritePin(PROFILE_PORT, PROFILE_PIN, GPIO_PIN_RESET);
}

// Connect logic analyzer to PC13
// Measure pulse duration = tick() execution time
```

### Adaptive Refresh Rate

```c
/**
 * @brief Adaptive refresh rate based on content changes
 */

static uint8_t prev_fb[1024];
static uint8_t dirty = 1;

void mark_dirty(void)
{
    dirty = 1;
}

void smart_update(void)
{
    if (!dirty) {
        // No changes, reduce update rate
        HAL_Delay(10);
        return;
    }
    
    // Content changed, stream aggressively
    for (int i = 0; i < 32; i++) {  // Full update in ~32 ticks @ mode 5
        ssd_tick();
    }
    
    // Save current state
    memcpy(prev_fb, ssd_get_framebuffer(), 1024);
    dirty = 0;
}
```

## Debugging and Diagnostics

### I2C Scanner

```c
void i2c_scan(void)
{
    printf("Scanning I2C bus...\n");
    
    for (uint8_t addr = 0x01; addr < 0x7F; addr++) {
        HAL_StatusTypeDef result;
        
        result = HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10);
        
        if (result == HAL_OK) {
            printf("Found device at address 0x%02X\n", addr);
        }
    }
    
    printf("Scan complete.\n");
}
```

### Framebuffer Dump

```c
void dump_framebuffer(void)
{
    uint8_t *fb = ssd_get_framebuffer();
    
    printf("Framebuffer dump:\n");
    for (uint16_t i = 0; i < 1024; i++) {
        printf("%02X ", fb[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
}
```

### Cursor Monitoring

```c
void monitor_cursor(void)
{
    static uint16_t prev_cursor = 0;
    uint16_t cursor = ssd_get_cursor();
    
    if (cursor < prev_cursor) {
        printf("Cursor wrapped: %u -> %u\n", prev_cursor, cursor);
    }
    
    prev_cursor = cursor;
}
```

## Integration Checklist

- [ ] I2C configured @ 400 kHz in CubeMX
- [ ] Pull-up resistors present (internal or external 4.7kΩ)
- [ ] Display power connected (VCC, GND)
- [ ] I2C address correct (verify with i2c_scan)
- [ ] ssd_init() returns HAL_OK
- [ ] ssd_tick() called periodically
- [ ] Mode set to reasonable value (4-6)
- [ ] Test pattern visible on screen after ssd_flush()

## FAQ

**Q: Can I use other I2C (I2C2, I2C3)?**  
A: Yes, just pass the corresponding handle to ssd_init().

**Q: How to increase FPS?**  
A: Use larger mode (6-10) or call tick() more frequently.

**Q: Does the driver affect other I2C devices?**  
A: No, if they're on different buses. On same bus - standard multim aster behavior.

**Q: Can I use with 128x32 displays?**  
A: Yes, but need to change SSD_HEIGHT and init commands (multiplex ratio).

**Q: Does it work with SH1106?**  
A: With minor changes: column offset +2, see README.md.

---

**Author**: Claude (Anthropic AI)  
**Date**: 2026-01-31
