# SSD1306 Non-blocking Streaming OLED Driver

**Version**: 1.0  
**Platform**: STM32F411 BlackPill  
**Display**: 128x64 monochrome OLED (SSD1306/SH1106)  
**Interface**: I2C @ 400 kHz (without DMA)

## Overview

This is a non-blocking streaming driver for monochrome OLED displays with SSD1306-compatible controllers. The driver cycles through an internal 1024-byte framebuffer and transfers a configurable number of bytes on each `tick()` call (1 to 1024 bytes per tick). The main constraint: **no DMA usage** - DMA channels are reserved for other subsystems.

### Key Features

- ✓ Cyclic streaming of 1024-byte framebuffer (128×64/8)
- ✓ 11 transfer modes (mode 0..10 → 1..1024 bytes per tick)
- ✓ Deterministic execution time (~0.05 ms to ~25 ms depending on mode)
- ✓ No DMA usage (keeps DMA available for other systems)
- ✓ Horizontal addressing mode for linear memory access
- ✓ Simple pixel-level drawing API

## Architecture

```
┌─────────────────────────────────────────────┐
│         User Application (main.c)           │
│  - Draws into framebuffer (ssd_set_pixel)   │
│  - Calls ssd_tick() in main loop            │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│          ssd_stream.c (Driver)              │
│  - Manages cursor (0..1023)                 │
│  - Sends chunk bytes per tick()             │
│  - Wraps at end of buffer                   │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│         HAL_I2C_Master_Transmit             │
│  - Blocking transmission (no DMA)           │
└─────────────────┬───────────────────────────┘
                  │
              I2C bus @ 400 kHz
                  │
              ┌───▼───┐
              │ OLED  │
              │Display│
              └───────┘
```

## Quick Start

### 1. Hardware Setup

- Connect OLED display to I2C1:
  - SCL → PB6 (or configured pin)
  - SDA → PB7 (or configured pin)
  - VCC → 3.3V or 5V (check your display)
  - GND → GND
- Pull-up resistors: 4.7kΩ (internal or external)

### 2. STM32CubeMX Configuration

**I2C1 Settings:**
- Mode: I2C
- I2C Speed Mode: Fast Mode
- I2C Clock Speed: 400000 Hz
- GPIO: Pull-up enabled (or external resistors)

**System Clock:**
- HCLK: 100 MHz (maximum for STM32F411)

### 3. Integration

Copy files to your project:
```
Core/Inc/ssd_stream.h
Core/Src/ssd_stream.c
```

### 4. Minimal Code Example

```c
#include "ssd_stream.h"

extern I2C_HandleTypeDef hi2c1;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_I2C1_Init();
    
    /* Initialize display */
    if (ssd_init(&hi2c1, 0x3C) != HAL_OK) {
        Error_Handler();
    }
    
    /* Set mode: 5 = 32 bytes/tick ≈ 0.8 ms */
    ssd_set_mode(5);
    
    /* Draw border */
    ssd_clear(0x00);
    for (uint8_t x = 0; x < 128; x++) {
        ssd_set_pixel(x, 0, 1);
        ssd_set_pixel(x, 63, 1);
    }
    for (uint8_t y = 0; y < 64; y++) {
        ssd_set_pixel(0, y, 1);
        ssd_set_pixel(127, y, 1);
    }
    
    /* Immediate flush */
    ssd_flush();
    
    /* Main loop */
    while (1)
    {
        ssd_tick();  /* Stream framebuffer */
        HAL_Delay(1);
    }
}
```

## API Reference

### Initialization

```c
HAL_StatusTypeDef ssd_init(I2C_HandleTypeDef *hi2c, uint8_t i2c_addr);
```
Initializes the display with horizontal addressing mode.
- **hi2c**: Pointer to HAL I2C handle (e.g., &hi2c1)
- **i2c_addr**: 7-bit I2C address (typically 0x3C or 0x3D)
- **Returns**: HAL_OK on success

### Mode Setting

```c
void ssd_set_mode(uint8_t mode);
```
Sets the number of bytes transferred per tick().
- **mode**: 0..10 (automatically clamped)
  - mode 0 → 1 byte/tick
  - mode 5 → 32 bytes/tick (recommended)
  - mode 10 → 1024 bytes/tick

### Streaming

```c
void ssd_tick(void);
```
**Main driver function.** Call periodically from main loop or timer.
- Sends chunk of bytes (≤ bytesPerTick)
- Automatically handles wrap at end of buffer
- Blocks for one I2C transaction only (~0.05–3 ms depending on mode)

### Framebuffer Operations

```c
uint8_t* ssd_get_framebuffer(void);
```
Returns pointer to 1024-byte framebuffer.

```c
void ssd_clear(uint8_t pattern);
```
Fills entire framebuffer with pattern.
- **pattern**: 0x00 = black, 0xFF = white

```c
void ssd_set_pixel(uint8_t x, uint8_t y, uint8_t color);
```
Sets a single pixel.
- **x**: 0..127
- **y**: 0..63
- **color**: 0 = black, 1 = white

### Immediate Update

```c
HAL_StatusTypeDef ssd_flush(void);
```
Sends entire framebuffer in one transaction (~25 ms).
**Warning**: This is a blocking operation!

### Debug

```c
uint16_t ssd_get_cursor(void);
```
Returns current cursor position (0..1023).

## Transfer Modes and Timing

### Mode Table (I2C @ 400 kHz)

| Mode | Bytes/tick | Approx. Time | Recommended Use |
|------|------------|--------------|-----------------|
| 0    | 1          | ~0.05 ms     | Minimal load, slow refresh |
| 1    | 2          | ~0.1 ms      | |
| 2    | 4          | ~0.15 ms     | |
| 3    | 8          | ~0.25 ms     | |
| 4    | 16         | ~0.4 ms      | Good balance |
| 5    | 32         | ~0.8 ms      | **Recommended mode** |
| 6    | 64         | ~1.6 ms      | Fast refresh |
| 7    | 128        | ~3.2 ms      | |
| 8    | 256        | ~6.4 ms      | |
| 9    | 512        | ~12.8 ms     | |
| 10   | 1024       | ~25 ms       | Full frame per tick |

### Full Screen Refresh Time

- **Mode 0**: 1024 ticks × 0.05 ms = **~51 ms** (~20 FPS)
- **Mode 5**: 32 ticks × 0.8 ms = **~26 ms** (~38 FPS)
- **Mode 10**: 1 tick × 25 ms = **~25 ms** (~40 FPS)

### Optimal Mode Selection

**For main loop with HAL_Delay(1):**
- Use **mode 4–5** (16–32 bytes)
- Provides ~30–40 FPS screen refresh
- Each tick() takes <1 ms

**For timer interrupts (1 kHz):**
- Use **mode 0–3** (1–8 bytes)
- Minimal interrupt load
- Or call with divider (every N interrupts)

**For maximum speed:**
- Use **mode 6–7** (64–128 bytes)
- Allows tick() >1 ms
- Full refresh in ~15–20 ms

## Framebuffer Structure

The framebuffer is organized as an array of 8 "pages" of 128 bytes each:

```
Byte:     0   1   2  ...  127 | 128 129 ... 255 | ... | 896 ... 1023
          ─────────────────────┼──────────────────┼─────┼──────────────
Page 0:   [  Row 0-7   (Y=0-7)  ]
Page 1:                          [  Row 8-15 (Y=8-15) ]
...
Page 7:                                                   [ Row 56-63 ]
```

Each byte represents 8 vertical pixels:
- Bit 0 (LSB) = Y = page*8 + 0
- Bit 1       = Y = page*8 + 1
- ...
- Bit 7 (MSB) = Y = page*8 + 7

### Example: Setting Pixel (60, 20)

```
X = 60, Y = 20
page = 20 / 8 = 2
bit  = 20 % 8 = 4
byte_index = 2 * 128 + 60 = 316

fb[316] |= (1 << 4);  // Set bit 4
```

## Graphics Utilities (ssd_utils.h)

Higher-level graphics primitives are available in `ssd_utils.h/c`:

```c
/* Shapes */
ssd_draw_line(x0, y0, x1, y1, color);
ssd_draw_rect(x, y, w, h, color);
ssd_fill_rect(x, y, w, h, color);
ssd_draw_circle(x0, y0, radius, color);
ssd_fill_circle(x0, y0, radius, color);
ssd_draw_triangle(x0, y0, x1, y1, x2, y2, color);

/* Text (built-in 5x7 font) */
ssd_draw_char(x, y, 'A', NULL, 1, 1);
ssd_draw_string(x, y, "Hello", NULL, 1, 1);

/* Advanced */
ssd_draw_progress_bar(x, y, w, h, percent);
ssd_draw_graph(x, y, data, len, max_val, h);
ssd_scroll_horizontal(pixels);
ssd_invert();
```

## Troubleshooting

### Issue: Display doesn't initialize (HAL_ERROR)

**Solutions:**
1. Check I2C address (try both 0x3C and 0x3D)
2. Verify SDA/SCL pull-up resistors (4.7kΩ to VCC)
3. Check display power supply (3.3V or 5V)
4. Use I2C Scanner to find address:

```c
for (uint8_t addr = 0x01; addr < 0x7F; addr++) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 1, 10) == HAL_OK) {
        printf("Found device at 0x%02X\n", addr);
    }
}
```

### Issue: Screen shows "garbage" or artifacts

**Solutions:**
1. Ensure horizontal addressing mode is used (check in ssd_init)
2. Call `ssd_flush()` after `ssd_init()` for synchronization
3. Verify column/page address range (should be 0..127, 0..7)
4. If using SH1106: needs column offset +2 (modify init_seq)

### Issue: Cursor wrap creates visual "glitches"

**Solutions:**
1. This is normal with large modes (6+) - use smaller mode
2. Verify cursor correctly resets to 0
3. Optionally send commands at wrap to resynchronize:

```c
if (ctx.cursor == 0) {
    /* Optionally: reset address */
    uint8_t reset_addr[] = {0x21, 0x00, 0x7F, 0x22, 0x00, 0x07};
    ssd_write_cmds(reset_addr, sizeof(reset_addr));
}
```

### Issue: tick() takes too long

**Solutions:**
1. Reduce mode (e.g., from 6 to 4–5)
2. Verify I2C frequency (should be 400 kHz)
3. Check SystemCoreClock (should be 100 MHz for STM32F411)
4. Ensure no other devices on I2C bus slowing transmission

### Issue: Image is flipped or mirrored

**Solutions:**
Modify commands in init_seq:
```c
/* Normal orientation: */
SSD_CMD_SEG_REMAP,      /* 0xA1 (flip horizontal) */
SSD_CMD_COM_SCAN_DEC,   /* 0xC8 (flip vertical) */

/* Reversed orientation: */
0xA0,  /* instead of 0xA1 */
0xC0,  /* instead of 0xC8 */
```

## Adaptation for SH1106/SH1107

If your display uses SH1106 instead of SSD1306:

1. **Column offset**: SH1106 has offset +2 columns
2. **Commands**: Slightly different (check datasheet)
3. **Modify init_seq**:

```c
/* For SH1106: */
SSD_CMD_COLUMN_ADDR, 0x02, 0x81,  /* Columns 2..129 (instead of 0..127) */
```

## Performance Optimization

### 1. Minimize Stack Usage

The driver uses a static buffer `g_tx_buffer[1025]` instead of stack allocation.

### 2. Atomic Operations

If calling `ssd_tick()` from both interrupt and main loop:

```c
void ssd_tick_safe(void) {
    __disable_irq();
    ssd_tick();
    __enable_irq();
}
```

### 3. Double Buffering (optional)

For smooth animation:

```c
static uint8_t fb_front[1024];
static uint8_t fb_back[1024];

void swap_buffers(void) {
    memcpy(fb_front, fb_back, 1024);
}
```

## Testing

Run the comprehensive test suite (see `ssd_stream_test.c`):

```c
run_all_tests(&hi2c1, 0x3C);
```

Test coverage includes:
- Initialization validation
- Mode setting bounds checking
- Pixel operations
- Cursor behavior and wrap
- Timing measurements (all modes)
- Data integrity
- Edge cases
- Stress testing (1000+ iterations)
- I2C communication validation

## Files

- **ssd_stream.h/c** - Core streaming driver (~300 lines)
- **ssd_utils.h/c** - Graphics utilities (~550 lines)
- **ssd_stream_example.c** - Usage examples (~250 lines)
- **ssd_stream_test.c** - Test suite (~450 lines)

## License

Free to use in commercial and non-commercial projects.

## References

- [SSD1306 Datasheet](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)
- [STM32F411 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00119316.pdf)
- [HAL I2C Documentation](https://www.st.com/resource/en/user_manual/dm00105879.pdf)

---

**Version**: 1.0  
**Date**: 2026-01-31  
**Author**: Claude (Anthropic AI)  
**Platform**: STM32F411 (ARM Cortex-M4)
