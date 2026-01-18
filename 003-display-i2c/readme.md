# 003-display-i2c - SSD1306 (SSD1106 with SSD1306 driver) I2C Test on STM32F4

## Description

This project demonstrates initialization and basic usage of an SSD1306 OLED display over I2C on an STM32F4 microcontroller using the HAL library. The code is intended as a minimal test and a starting point for further development.

## What was implemented

- MCU initialization and HAL setup (HAL_Init()).
- Enabling the Floating Point Unit (FPU) via the CPACR register.
- System clock configuration using the internal HSI oscillator and PLL.
- GPIO initialization (MX_GPIO_Init()).
- I2C1 initialization for communication with the SSD1306 (MX_I2C1_Init()).
- Integration of the SSD1306 driver headers (ssd1306.h and ssd1306_fonts.h).
- SSD1306 initialization sequence and status check (ssd1306_Init()).
- Basic graphics and text rendering:
  - Clear screen (ssd1306_Fill(Black)).
  - Draw a border rectangle.
  - Display text lines: "I2C OK", "SSD1306 Test", "darkfrei", and the date 2026-01-17.
  - Draw small decorative corner lines.
  - Update the display with ssd1306_UpdateScreen().
- Main loop is empty; the display remains in a static state after initialization.

## Result

- I2C interface with the SSD1306 is verified to work.
- The display initializes and shows text and simple graphics.

## Purpose of the project

- Provide a minimal, working example of an SSD1306 connected to an STM32 via HAL and I2C.
- Serve as a basis for adding dynamic data, menus, or sensor output.

## Next steps (ideas)

- Add dynamic content updates.
- Integrate sensors or timers and display live readings.
- Implement a simple UI or menu system.
- Optimize screen update routines to reduce traffic over I2C.


