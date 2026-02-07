# Blackpill Test 003: SSD1306 OLED Display via I2C

This project tests and demonstrates how to use an SSD1306 OLED display with a **Blackpill STM32** microcontroller using the **I2C** communication protocol.

---

## What This Test Does

This test connects a **128×64 pixel OLED display** to the Blackpill board and shows:

- A blinking LED on **PC13** (the onboard LED)
- The LED status (**ON / OFF**) on the display
- UPS (Updates Per Second) - how many update operations (`while (1)`) happen each second
- A static border and title on the display

The main goal is to verify reliable I2C communication and to experiment with **optimized, non-blocking screen updates**.

---

## What You’ll Need

### Hardware

1. STM32F411CE Blackpill board
2. SSD1306 OLED display (128×64, I2C interface)
3. Breadboard and jumper wires
4. USB cable for programming and power

### Software

1. STM32CubeIDE (or another STM32-compatible IDE)
2. STM32CubeMX for initial project configuration
3. STM32 programming tools (ST-Link Utility, OpenOCD, etc.)

---

## Wiring Connections

Connect the OLED display to the Blackpill as follows:

| SSD1306 Pin | Blackpill Pin | Function |
|------------|--------------|----------|
| VCC        | 3.3V         | Power    |
| GND        | GND          | Ground   |
| SCL        | PB8          | I2C Clock |
| SDA        | PB9          | I2C Data |

> **Note**  
> Some SSD1306 modules use different pin layouts. Always check the datasheet of your specific module.

---

## How to Set Up and Use

### 1. Project Setup

- Create a new STM32CubeMX project for **STM32F411CEU6 Blackpill**
- Enable **I2C1** on pins **PB8 / PB9**
- **Connevtivity**: **I2C1**: Fast Mode, 400 000 Hz
- Enable **GPIO output** for **PC13** (onboard BLUE LED)
- Clock Configuration (HSI RC 16 MHz; System Clock Max: HSI; HCLK 16 MHz)
- Project Manager:
-- Application Structure: Basic
-- Toolchain / IDE: STM32CubeIDE
- Code Generator: Generatr peripheral initialization as a pair of `.c / .h` files per peripheral
- Generate code for your IDE

### 2. Adding the Display Library

Copy the following files into your project:

- Src/`ssd1306.c` - main OLED driver
- Src/`ssd1306_fonts.c` - font definitions
- Inc/`ssd1306.h` - main OLED driver header
- Inc/`ssd1306_fonts.h` - font definitions header
- Inc/`ssd1306_conf.h` - driver configuration

### 3. Configuration Adjustments

In `ssd1306_conf.h`:

- Ensure `STM32F4` is defined
- Ensure `SSD1306_USE_I2C` is enabled
- Verify the I2C handle is `hi2c1`
- Confirm the display address is `(0x3C << 1)`
- Select the screen update chunk size (see below)

### 4. Building and Flashing

- Build the project
- Connect the Blackpill using ST-LinkV2
- Flash the firmware
- The display should show **"Test SSD1306"**, LED state, and the UPS counter

---

## How It Works

### Main Loop

The application runs an infinite loop that:

1. Toggles the LED every **500 ms**
2. Updates the display text with the current LED state
3. Continuously sends small chunks of framebuffer data to the OLED
4. Calculates and displays **UPS (Updates Per Second)**

---

## Screen Update Optimization

This project demonstrates two important optimization techniques.

### 1. Chunked Updates

Instead of sending the entire framebuffer (1024 bytes) in one blocking operation, the code sends **small chunks** (16–128 bytes).

This:
- Reduces blocking time
- Keeps the main loop responsive
- Allows higher update frequency

### 2. Dirty Region Tracking

The driver tracks which parts of the framebuffer have changed and only updates those regions.

Benefits:
- Less I2C traffic
- Higher UPS
- Lower power consumption

---

## Configuration Options

In `ssd1306_conf.h`, adjust the chunk size:

```c
// smaller chunks = higher ups, less blocking
#define SSD1306_UPDATE_CHUNK_SIZE_POW 4   // 2^4 = 16 bytes per update

// larger chunks = fewer transfers, more blocking
// #define SSD1306_UPDATE_CHUNK_SIZE_POW 7 // 2^7 = 128 bytes per update
```

The **UPS value shown on the display** indicates how many update operations occur per second. Higher UPS generally results in smoother animations.

---

## Technical Details

### I2C Communication

I2C (Inter‑Integrated Circuit) uses two signals:

- **SCL** - clock
- **SDA** - data

Multiple devices can share the bus. The SSD1306 typically uses address **0x3C**.

### Screen Buffer (Framebuffer)

The OLED uses a **1024‑byte framebuffer**:

- 128 × 64 pixels
- 1 bit per pixel
- Organized as pages (8 vertical pixels per byte)

The MCU modifies the framebuffer in RAM and then transfers it to the display.

### Page Addressing

The SSD1306 divides the screen into **pages**:

- Each page = 8 vertical pixels
- A 64‑pixel display has **8 pages**

Updates specify both the **page number** and the **column address**.

---

## Common Issues and Solutions

### Display Shows Nothing

- Verify power (3.3 V, not 5 V; or see the datasheet of your screen)
- Confirm I2C address (`0x3C`, sometimes `0x78`)
- Check SDA/SCL wiring
- Ensure pull‑up resistors are present (if relevant)

### Display Shows Garbage

- Try lowering I2C speed to **100 kHz**
- Verify the initialization sequence in `ssd1306_Init()`
- Check stack and heap sizes
- Adjust sonstants in the ssd1306_conf.h: SSD1306_X_OFFSET 2 or 0

### Low UPS

- Reduce chunk size (16 or 32 bytes)
- Increase I2C speed to **400 kHz** if wiring allows
- Disable dirty tracking if the whole screen updates every frame

---

## Project Structure

```text
003-display-i2c/
│
├── Inc/
│   ├── ssd1306.h
│   ├── ssd1306_conf.h
│   ├── ssd1306_fonts.h
│   └── (other .h files)
├── Src/
│       ├── ssd1306.c
│       ├── main.c
│       └── (other .c files)
├── Drivers/
│   ├── STM32 HAL drivers (must be created from CubeMX)
│   └── CMSIS drivers (must be created from CubeMX)
└── (other files: .cproject, .mxproject, .project, .ioc)
```

---

## Next Steps

After this test is working, you can:

- Add more graphics (lines, circles, bitmaps)
- Build a menu system
- Display sensor data (temperature, pressure, humidity)
- Implement double buffering
- Use DMA for fully non‑blocking screen updates

---

## References

- [SSD1306 Datasheet](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)
- [STM32F411CE Reference Manual](https://www.st.com/resource/en/datasheet/stm32f411ce.pdf)
- [I2C Protocol Specification](https://www.st.com/resource/en/application_note/dm00072315-using-the-i2c-bus-interface-in-stm32f0-stm32f3-stm32f4-stm32f7-and-stm32l0-stm32l1-stm32l4-series-stmicroelectronics.pdf) 
- [STM32Cube HAL Documentation](https://www.st.com/resource/en/user_manual/um1725-description-of-stm32f4-hal-and-lldrivers-stmicroelectronics.pdf)

---

**Last updated:** 2026-02-07
**Status:** Complete
