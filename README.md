# BlackPill STM32F411CE Learning Projects

Personal collection of STM32 projects for WeAct BlackPill F411CE board. This repository tracks my progress in embedded development, from basic GPIO control to more advanced peripherals and protocols.

## Hardware

**Development Board**: WeAct STM32F411CEU6 BlackPill

**Key Specifications**:
- MCU: STM32F411CEU6
- Core: ARM Cortex-M4F with FPU
- Clock: Up to 100MHz
- Flash: 512KB
- RAM: 128KB
- External Crystal: 25MHz

**Programmer**: ST-Link V2

## Development Environment

### Required Software

- STM32CubeMX - for peripheral configuration and code generation
- STM32CubeIDE - main development IDE with integrated toolchain
- ST-Link drivers - for programming and debugging

### Hardware Setup

ST-Link V2 connections to BlackPill:

```
ST-Link V2          BlackPill F411
----------          --------------
SWDIO       ----->  DIO
SWCLK       ----->  DCLK
GND         ----->  GND
3.3V        ----->  3V3 (optional)
```

---

## Projects

Each project is self-contained in its own directory with complete source code and configuration files.

### 001-blink - LED Blink

Basic GPIO output control. Toggles LED on PA5 every 500ms.

[001-blink/](001-blink) -- my Hello World

Topics covered:
- GPIO configuration
- HAL library basics
- System clock setup
- Basic project structure

Status: Complete

---

### 002-breath - LED Breathing (Software PWM)

Smooth LED breathing effect implemented using software PWM and precise microsecond delays based on the DWT cycle counter. The brightness is changed gradually while maintaining a continuous PWM signal to avoid visible flicker.

[002-breath](002-breath) -- link to the project

Key characteristics:
- Software-generated PWM on GPIO pin PC13 (onboard LED, inverted logic)
- PWM frequency approximately 1 kHz (no visible PWM flicker)
- Brightness updated independently from PWM timing
- Uses DWT CYCCNT for accurate microsecond delays

Topics covered:
- Software PWM implementation
- Duty cycle control
- Separation of PWM timing and fade timing
- DWT cycle counter usage
- Timing-related visual artifacts and their mitigation

Status: Complete

---

### 003-display-i2c - SSD1306 I2C Test

This project: 003-display-i2c

- Minimal SSD1306 I2C example for STM32F4 using HAL.
- Demonstrates display initialization, basic text and graphic routines, and I2C configuration.

[003-display-i2c](003-display-i2c)

Status: Complete

---

### Future Projects

More projects will be added as I progress through learning STM32 development.

**Planned topics**:
- UART communication
- Timers and PWM
- ADC input
- I2C peripherals
- SPI communication
- Interrupts
- DMA transfers
- Real-time clock
- Low power modes

## Notes

This is a learning repository. Code may not follow production best practices as it is focused on understanding fundamentals and experimenting with STM32 features.

Each project includes:
- Complete STM32CubeIDE project
- STM32CubeMX configuration file
- Project-specific README with details
- Commented code for learning purposes

## Build Instructions

### Opening a Project

1. Launch STM32CubeIDE
2. File -> Open Projects from File System
3. Select the project directory
4. Click Finish

### Building and Flashing

1. Connect ST-Link V2 to BlackPill
2. Connect ST-Link V2 to PC
3. Build: Ctrl+B
4. Flash and Debug: F11

## Common Configuration

All projects use the same base configuration unless otherwise noted:

- System clock: 100MHz (from 25MHz HSE via PLL)
- Debug interface: Serial Wire (SWD)
- HAL library for peripheral drivers
- FreeRTOS included where needed

## Resources

- [STM32F411 Product Page](https://www.st.com/en/microcontrollers-microprocessors/stm32f411.html)
- [BlackPill GitHub](https://github.com/WeActTC/MiniSTM32F4x1)
- [STM32F411 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00119316.pdf)
- [STM32F411 Datasheet](https://www.st.com/resource/en/datasheet/stm32f411ce.pdf)

## Progress Log

- 2025-01-10: Initial setup, first LED blink project working
- Future entries will be added as projects progress

## License

Educational purposes. STMicroelectronics HAL library components are subject to their respective licenses.
