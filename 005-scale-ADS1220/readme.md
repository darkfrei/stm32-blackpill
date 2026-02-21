# Digital Scale with ADS1220 and SH1106

## Project Purpose

This project implements a high resolution digital scale based on an STM32 microcontroller and the ADS1220 24 bit analog to digital converter. The goal is to demonstrate precise weight measurement using a load cell, integer only signal processing, modular driver architecture, and real time visualization on an OLED display.

The firmware is designed for educational and practical embedded development. It shows how to combine SPI, I2C, GPIO, and timer peripherals in a structured STM32CubeMX generated project.

## What the Project Does

The system performs the following tasks:

- Reads differential analog voltage from a load cell using the ADS1220.
- Converts the 24 bit ADC result into a signed integer value.
- Applies tare offset subtraction.
- Scales the result using a calibration divisor.
- Displays weight in grams with one decimal place.
- Shows diagnostic information such as raw ADC value and samples per second.
- Allows tare operation via a push button.

All weight calculations are performed using integer arithmetic. The variable weight_grams_x10 represents the weight in grams multiplied by ten.

## System Architecture

The firmware is divided into logical modules:

- main.c: Application logic and integration of all modules.
- ads1220.c and ads1220.h: ADS1220 driver with hardware abstraction via function pointers.
- sh1106.c and sh1106.h: OLED display driver.
- sh1106_fonts.h: Font definitions for text rendering.
- EC11.c and EC11.h: Rotary encoder driver.

The ADS1220 driver is hardware independent. The application assigns low level functions to the ADS1220 handle:

- csLow and csHigh for chip select control.
- spiTxRx for SPI communication.
- drdyRead for data ready pin status.

This approach allows reuse of the driver on different STM32 boards without modification.

## Hardware Overview

Typical hardware components:

- STM32 microcontroller (example: STM32F411).
- ADS1220 24 bit ADC connected via SPI.
- Load cell in full bridge configuration.
- SH1106 128x64 OLED display connected via I2C.
- Push button for tare function.
- Optional EC11 rotary encoder.

ADS1220 key parameters used in this project:

- Gain: 128.
- Internal reference voltage: 2.048 V.
- 24 bit delta sigma ADC architecture.
- DRDY pin used to detect conversion completion.

## STM32 Peripheral Configuration

The following peripherals are used:

GPIO:
- Output for status LED.
- Input with pull up for tare button.
- Output for ADS1220 chip select.
- Input for ADS1220 DRDY.

SPI1:
- Master mode.
- 8 bit data size.
- Software controlled chip select.
- Used for ADS1220 register access and data read.

I2C1:
- Fast mode, typically 400 kHz.
- Used for SH1106 OLED communication.

TIM2:
- Encoder interface mode if EC11 is used.
- 16 bit counter.

System clock:
- External HSE oscillator.
- PLL configured for high performance system clock.

## Measurement Principle

The load cell produces a small differential voltage proportional to applied force. The ADS1220 amplifies this signal using programmable gain and converts it to a 24 bit signed digital value.

The firmware processes data as follows:

1. Read raw ADC value into adc_raw.
2. Subtract tare_offset to obtain adc_code.
3. Compute weight_grams_x10 using:

   weight_grams_x10 = (adc_code * 10) / calibration_divisor

The calibration_divisor is determined experimentally by placing a known reference mass on the scale and adjusting until the displayed value matches the real weight.

Integer arithmetic ensures deterministic execution time and avoids floating point overhead.

## Tare Function

The tare button is connected to a GPIO configured with pull up. When pressed, the current adc_raw value is stored in tare_offset.

All subsequent measurements are calculated relative to this stored offset. Basic debounce is implemented using a short delay and state comparison.

## Display Handling

The SH1106 display driver is used in buffered mode. Static elements such as borders and labels are drawn once during initialization.

Dynamic fields are updated periodically:

- Weight in grams with one decimal.
- Raw ADC value.
- ADC value after tare.
- Scaled x10 value.
- Calibration divisor and samples per second.

The update period is controlled by UPDATE_DELAY_MS.

## Sampling Rate Calculation

Each successful ADC read increments sample_count. Once per second, the firmware copies sample_count into samples_per_sec and resets the counter.

This provides a runtime indication of the effective sampling speed.

## Main Loop Operation

Inside the infinite loop:

- Check if new ADC data is available.
- Read and process measurement.
- Update tare state if button pressed.
- Compute samples per second.
- Refresh display periodically.

The design avoids blocking delays except during initialization and debounce.

## Interaction Between Modules

The main application calls:

- ADS1220_Init during startup.
- ADS1220_ReadData for measurement.
- ADS1220_DataReady for DRDY polling.
- SH1106_Init and SH1106_UpdateScreen for display control.
- EC11_Init and HAL_TIM_Encoder_Start for encoder support.

Drivers do not directly depend on each other. All integration is handled in main.c.

## Calibration Procedure

1. Power on the system without load and press tare.
2. Place a known reference mass on the scale.
3. Observe weight_grams_x10 or displayed grams.
4. Adjust calibration_divisor in firmware.
5. Rebuild and flash until the displayed value matches the reference mass.

## Detailed STM32CubeMX Configuration Used In This Project

The following settings are taken directly from the provided CubeMX project file.

Microcontroller and Clock

- External crystal HSE: 25 MHz.
- PLL Source: HSE.
- PLLM: 12.
- PLLN: 96.
- PLLCLK Frequency: 100 MHz.
- SYSCLK Source: PLLCLK.
- SYSCLK Frequency: 100 MHz.
- AHB Frequency: 100 MHz.
- APB1 Prescaler: HCLK DIV2.
- APB1 Frequency: 50 MHz.
- APB2 Frequency: 100 MHz.

GPIO Configuration

- PC13: GPIO Output, used as status LED.
- PB0: GPIO Output, high speed, initial state SET, used as ADS1220 chip select.
- PB1: GPIO External Interrupt line 1, used as ADS1220 DRDY.
- PA3: GPIO External Interrupt on falling edge with pull up, used as tare button.
- PA4: GPIO External Interrupt on falling edge with pull up.

SPI1 Configuration for ADS1220

- Mode: Master.
- Direction: 2 lines full duplex.
- Clock Phase: 2nd edge.
- Baud Rate Prescaler: 256.
- Calculated SPI Speed: approximately 390.625 kbit per second.
- Software controlled chip select via PB0.

I2C1 Configuration for SH1106

- Mode: I2C Fast Mode.
- Pins: PB6 SCL, PB7 SDA.
- Used for OLED display communication.

TIM2 Configuration for EC11 Encoder

- Encoder Mode: TI1 and TI2.
- Period: 65535.
- Auto Reload Preload: Enabled.
- Clock Division: DIV1.
- Input Capture 1 Polarity: Rising.
- Input Capture 2 Polarity: Rising.
- IC1 and IC2 Prescaler: DIV1.
- IC1 and IC2 Filter: 0.

Project Structure

The initialization functions are generated by CubeMX in the following order:

- SystemClock_Config.
- MX_GPIO_Init.
- MX_I2C1_Init.
- MX_TIM2_Init.
- MX_SPI1_Init.

These settings ensure stable 100 MHz system operation, SPI communication with ADS1220 at a safe speed, fast I2C communication with the OLED, and hardware based quadrature decoding using TIM2.

## Possible Extensions

- Moving average digital filtering.
- Median filter for noise suppression.
- Automatic multi point calibration.
- EEPROM storage of calibration data.
- Menu system using rotary encoder.
- Overload detection and warning.

This project provides a structured and modular foundation for building precision measurement systems on STM32 using high resolution ADC devices.

