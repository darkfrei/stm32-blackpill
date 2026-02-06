# SSD1306 Non-blocking Streaming OLED Driver

**Version**: 1.0  
**Platform**: STM32F411 BlackPill  
**Display**: 128x64 monochrome OLED (SSD1306/SH1106)  
**Interface**: I2C @ 400 kHz (without DMA)

## Project Contents

### Core Driver Files

1. **ssd_stream.h** / **ssd_stream.c**
   - Core driver with framebuffer streaming
   - API for initialization, mode control, and pixel drawing
   - Size: ~300 lines of code

2. **ssd_utils.h** / **ssd_utils.c**
   - High-level graphics primitives
   - Lines, rectangles, circles, triangles
   - Built-in 5x7 ASCII font
   - Size: ~550 lines of code

### Examples and Tests

3. **ssd_stream_example.c**
   - Driver usage examples
   - Test patterns (checkerboard, stripes, border)
   - Templates for main() and various scenarios
   - Size: ~250 lines of code

4. **ssd_stream_test.c**
   - Comprehensive test suite (9 test cases)
   - Performance and timing measurements
   - Data integrity and edge case validation
   - Size: ~450 lines of code

### Documentation

5. **README.md**
   - Detailed architecture and API guide
   - Timing characteristic tables
   - Troubleshooting and FAQ
   - Size: ~500 lines

6. **INTEGRATION.md**
   - Step-by-step integration guide
   - Real-world application examples (system monitor)
   - FreeRTOS and timer integration
   - Size: ~400 lines

7. **PROJECT_OVERVIEW.md** (this file)
   - Quick project description

## Quick Start

### Minimal Code to Run

```c
#include "ssd_stream.h"

extern I2C_HandleTypeDef hi2c1;

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_I2C1_Init();  // 400 kHz
    
    // Initialize
    ssd_init(&hi2c1, 0x3C);
    ssd_set_mode(5);  // 32 bytes/tick
    
    // Draw border
    ssd_clear(0x00);
    for (uint8_t x = 0; x < 128; x++) {
        ssd_set_pixel(x, 0, 1);
        ssd_set_pixel(x, 63, 1);
    }
    for (uint8_t y = 0; y < 64; y++) {
        ssd_set_pixel(0, y, 1);
        ssd_set_pixel(127, y, 1);
    }
    
    ssd_flush();  // Immediate display
    
    // Main loop
    while (1) {
        ssd_tick();  // Stream framebuffer
        HAL_Delay(1);
    }
}
```

## Key Features

### ✅ Functionality

- ✓ Cyclic streaming of 1024-byte framebuffer
- ✓ 11 transfer modes (1..1024 bytes per tick)
- ✓ Deterministic execution time (<0.05 ms to ~25 ms)
- ✓ No DMA usage (DMA available for other systems)
- ✓ Horizontal addressing mode for linear memory
- ✓ API for pixel graphics and screen clearing

### 📊 Performance

| Mode | Bytes/tick | Tick Time | FPS (full screen) |
|------|------------|-----------|-------------------|
| 0    | 1          | ~0.05 ms  | ~20 FPS          |
| 4    | 16         | ~0.4 ms   | ~35 FPS          |
| 5    | 32         | ~0.8 ms   | ~38 FPS (recommended) |
| 6    | 64         | ~1.6 ms   | ~40 FPS          |
| 10   | 1024       | ~25 ms    | ~40 FPS          |

### 🎨 Graphics Primitives (ssd_utils)

- Lines (horizontal, vertical, arbitrary)
- Rectangles (outline and filled)
- Circles (outline and filled)
- Triangles (outline and filled)
- Bitmap/XBM images
- Text (built-in 5x7 font)
- Progress bars, graphs
- Scrolling, inversion

## How to Use

### Option 1: Minimal Integration

Copy only core files:
```
Core/Inc/ssd_stream.h
Core/Src/ssd_stream.c
```

### Option 2: Full Integration with Graphics

Copy all driver files:
```
Core/Inc/ssd_stream.h
Core/Inc/ssd_utils.h
Core/Src/ssd_stream.c
Core/Src/ssd_utils.c
```

### Option 3: With Examples and Tests

Also add example files:
```
Core/Src/ssd_stream_example.c  // For quick start
Core/Src/ssd_stream_test.c     // For validation
```

## STM32CubeMX Setup

1. **I2C1 Configuration:**
   - Speed Mode: Fast Mode (400 kHz)
   - Clock Speed: 400000 Hz
   - GPIO: SCL (PB6), SDA (PB7)
   - Pull-up: Internal or External 4.7kΩ

2. **System Clock:**
   - HCLK: 100 MHz (maximum for F411)

3. **Optional (for tests):**
   - Enable DWT (for precise timing measurements)
   - UART (for printf debugging)

## Additional Documentation

- **README.md** — detailed architecture and API description
- **INTEGRATION.md** — integration guide for projects
- Code comments — documentation for each function

## Requirements

- **Hardware:**
  - STM32F411CEU6 (BlackPill) or compatible
  - SSD1306/SH1106 OLED display 128x64
  - Pull-up resistors 4.7kΩ on I2C lines (optional)

- **Software:**
  - STM32CubeIDE / Keil / Makefile
  - STM32 HAL Library
  - Standard C library (string.h, stdlib.h)

## License

Free to use in commercial and non-commercial projects.

## Links

- [SSD1306 Datasheet](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)
- [STM32F411 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00119316.pdf)

---

**Developed**: 2026-01-31  
**Author**: Claude (Anthropic AI)  
**For**: STM32 Embedded Development
