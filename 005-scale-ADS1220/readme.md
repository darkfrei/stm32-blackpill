# Digital Scale with ADS1220 and SH1106

## Project Purpose

This project implements a high resolution digital scale based on an STM32 microcontroller and the ADS1220 24 bit analog to digital converter. The goal is to demonstrate precise weight measurement using a load cell, integer only signal processing, modular driver architecture, and real time visualization on an OLED display.

The firmware is designed for educational and practical embedded development. It shows how to combine SPI, I2C, GPIO, and timer peripherals in a structured STM32CubeMX generated project.

## What the Project Does

The system performs the following tasks:

- Reads differential analog voltage from a load cell using the ADS1220.
- Converts the 24 bit ADC result into a signed integer value.
- Applies tare offset subtraction.
- Scales the result using a calibration divisor to produce grams with one decimal place.
- Displays weight and diagnostic information on an OLED display.
- Shows the current operating mode permanently in the top bar of the display.
- Allows tare via the Confirm button.
- Allows live adjustment of the calibration divisor using a rotary encoder in Calibrate mode.
- Saves and restores the calibration divisor to and from internal Flash memory.

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
- EC11 rotary encoder with push button.
- Confirm button and Back button.

ADS1220 key parameters used in this project:

- Gain: 128.
- Internal reference voltage: 2.048 V.
- 24 bit delta sigma ADC architecture.
- DRDY pin used to detect conversion completion.

## STM32 Peripheral Configuration

The following peripherals are used:

GPIO:
- PC13: Output, status LED.
- PB0: Output, ADS1220 chip select.
- PB1: Input, ADS1220 DRDY.
- PA2: Input with pull up, Encoder Push button.
- PA3: Input with pull up, Confirm button.
- PA4: Input with pull up, Back button.

SPI1:
- Master mode.
- 8 bit data size.
- Software controlled chip select via PB0.
- Used for ADS1220 register access and data read.

I2C1:
- Fast mode, 400 kHz.
- Pins: PB6 SCL, PB7 SDA.
- Used for SH1106 OLED communication.

TIM2:
- Encoder interface mode for EC11.
- 16 bit counter, period 65535.
- Both channels rising edge, no prescaler, no filter.

System clock:
- External HSE crystal: 25 MHz.
- PLL: PLLM 12, PLLN 96.
- SYSCLK: 100 MHz.
- APB1: 50 MHz, APB2: 100 MHz.

## Operating Modes

The firmware operates in two modes. The current mode is always visible in the top bar of the display, shown with an inverted white background.

### Scale Mode

This is the default mode after power on.

- Confirm (PA3): stores the current raw ADC value as the tare offset. All subsequent weight readings are relative to this value.
- Encoder Push (PA2): enters Calibrate mode. The current divisor is saved internally so it can be restored if the calibration is cancelled.
- Encoder rotation: ignored in this mode.
- Bottom display line shows: OK=tare  push=cal

### Calibrate Mode

Used to adjust the calibration divisor while observing the live weight reading.

- Encoder rotation: changes calibration_divisor by one per detent. The displayed weight updates immediately.
- Confirm (PA3): saves the current divisor to internal Flash and returns to Scale mode.
- Back (PA4): discards all changes, restores the divisor that was active before entering Calibrate mode, and returns to Scale mode.
- Bottom display line shows: OK=save  back=undo

A brief notification message appears at the bottom of the screen after each action: Tared, Saved to Flash, or Cancelled.

## Flash Persistent Storage

The calibration divisor is stored in the last sector of internal Flash memory. The storage layout is a simple structure containing a magic number, the divisor value, and an XOR checksum for validation.

On every power on, the firmware reads this sector. If the magic number and checksum are valid, the stored divisor is loaded automatically. If the sector is blank or corrupted, the firmware continues with the compiled default value.

The Flash sector and base address must match your specific MCU:

- STM32F401: FLASH_SECTOR_5 at address 0x08020000.
- STM32F405 and F407 (1 MB flash): FLASH_SECTOR_7 at address 0x08060000.
- STM32F411 (512 KB Flash): FLASH_SECTOR_7 at address 0x08060000.
- STM32F446 (512 KB flash): FLASH_SECTOR_7 at address 0x08060000.

These are defined at the top of main.c:

    #define FLASH_STORAGE_SECTOR    FLASH_SECTOR_5
    #define FLASH_STORAGE_ADDR      0x08020000U

## Measurement Principle

The load cell produces a small differential voltage proportional to applied force. The ADS1220 amplifies this signal using programmable gain and converts it to a 24 bit signed digital value.

The firmware processes data as follows:

1. Read raw ADC value into adc_raw.
2. Subtract tare_offset to obtain adc_code.
3. Compute weight_grams_x10 using:

   weight_grams_x10 = (adc_code * 10) / calibration_divisor

The calibration_divisor represents the number of ADC counts per gram. Integer arithmetic ensures deterministic execution time and avoids floating point overhead.

## Display Layout

The 128x64 OLED display is fully redrawn each update cycle. The layout is fixed:

- Row 0 to 11: Mode bar. Inverted background, shows SCALE or CALIBRATE.
- Row 13: Weight in grams with one decimal, or a tare prompt if tare has not been performed.
- Row 23: Raw ADC value (adc_raw).
- Row 33: Net ADC value after tare subtraction (adc_code).
- Row 43: Current calibration divisor and samples per second.
- Row 53: Context hint for current mode, or a temporary notification message.

## Calibration Procedure

1. Power on the system with no load on the scale.
2. Press Confirm to tare.
3. Place a known reference mass on the scale.
4. Press Encoder Push to enter Calibrate mode.
5. Rotate the encoder until the displayed weight matches the reference mass.
6. Press Confirm to save the divisor to Flash and return to Scale mode.

If the result is unsatisfactory, re enter Calibrate mode and adjust further. Press Back at any point to discard changes without saving.

## Sampling Rate Calculation

Each successful ADC read increments sample_count. Once per second the firmware copies sample_count into samples_per_sec and resets the counter. This provides a runtime indication of the effective sampling speed, displayed on the screen.

## Main Loop Operation

Inside the infinite loop:

- Read and process ADC measurement if available.
- Poll all three buttons for edge detection.
- Read encoder delta.
- Execute the state machine for the current mode.
- Expire notification banners after 1.2 seconds.
- Refresh the display every UPDATE_DELAY_MS milliseconds.

The design avoids blocking delays except during initialization.

## Interaction Between Modules

The main application calls:

- ADS1220_Init during startup.
- ADS1220_ReadData for measurement.
- ADS1220_DataReady for DRDY polling.
- SH1106_Init and SH1106_UpdateScreen for display control.
- EC11_Init and HAL_TIM_Encoder_Start for encoder support.

Drivers do not directly depend on each other. All integration is handled in main.c.

## Possible Extensions

- Moving average or median filter for noise suppression.
- Automatic multi point calibration.
- Overload detection and warning.
- Adjustable encoder step size for faster coarse and slower fine divisor tuning.
- Unit switching between grams and ounces.
