# Project 4: Rotary Encoder EC11

This project demonstrates how to use an EC11 mechanical rotary encoder with an STM32 microcontroller (example: BlackPill STM32F411). The encoder rotation is decoded by a hardware timer in encoder interface mode, and the push button is handled via GPIO with software debouncing. An SSD1306 (or SH1106) OLED display is used for visualization.

---

## Project Overview

- `encoder_EC11.h` - public API and data structures of the EC11 driver.
- `encoder_EC11.c` - driver implementation (tick accumulation, direction detection, button debounce).

**EC11**: A common mechanical rotary encoder with two quadrature outputs (A and B) and an integrated push button.

**Quadrature signals (A/B)**: Two square-wave signals shifted by 90 degrees. By comparing their phase, rotation direction can be detected.

**Tick**: A single raw increment or decrement produced by the hardware encoder interface.

**Detent / Step**: A mechanical click position of the encoder. One step usually corresponds to several ticks (commonly 4).

**TIM (Timer)**: A hardware timer peripheral in STM32. TIM2 is used here in encoder interface mode.

**Encoder interface mode**: A special timer mode where the hardware decodes A/B signals and updates the counter automatically.

**GPIO**: General Purpose Input Output pin, used here for the encoder push button and LED.

**HAL**: Hardware Abstraction Layer provided by ST. It simplifies peripheral access.

**Debounce**: Filtering technique to ignore rapid signal changes caused by mechanical contacts.

**Wraparound**: When a timer counter reaches its maximum value and overflows back to zero. The driver compensates for this automatically.

---

## Wiring and Hardware Notes

Typical connections for BlackPill STM32F411:

- Encoder A: `PA0` (TIM2_CH1)
- Encoder B: `PA1` (TIM2_CH2)
- Encoder button: `PA2` (GPIO input with pull-up)
- OLED SSD1306: I2C1, example `PB6` (SCL), `PB7` (SDA)
- Blue LED: `PC13`

Notes:
- Encoder A and B should have pull-ups (internal or external).
- The button is usually active-low (pressed = 0).
- SSD1306 requires proper I2C initialization, ensure clock and data pins are configured correctly.

---

## STM32CubeMX Configuration

1. Create a new project and select your MCU (e.g., STM32F411CEU6 Blackpill).
2. Configure pins:
   - `PC13` - GPIO Output (Blue LED)
   - Encoder A: `PA0` - TIM2_CH1
   - Encoder B: `PA1` - TIM2_CH2
   - Encoder Button: `PA2` - GPIO Input with Pull-Up
   - Optional Buttons: `PA3`, `PA4` - GPIO Input with Pull-Up
   - I2C for OLED Display (SSD1306 or SH1106): `PB6` (SCL), `PB7` (SDA)
3. Configure Timers: TIM2 for encoder:
   - Mode: "Encoder Mode"
   - Encoder Mode: TI1 and TI2
   - Counter Mode: Up
   - Prescaler: 0
   - Counter Period: 65535
   - Auto-reload Preload: Enable
   - Input Capture 1/2 Polarity: Rising Edge
   - Input Capture 1/2 Selection: Direct
   - Filter: 0; optional 0–15 (adjust for signal noise)
4. Configure GPIOs for buttons with Pull-Up.
5. Configure Connectivity:I2C1 for SSD1306:
   - Mode: I2C
   - Clock Speed: 400 kHz (Fast Mode)
   - Duty Cycle: 2
   - Own Address: 0 (not used)
   - Addressing Mode: 7-bit
   - Enable Analog Filter
   - GPIO pins: `PB6` = SCL, `PB7` = SDA
   - Pull-up: Internal pull-ups or external 4.7kΩ–10kΩ resistors recommended
6. Clock Configuration: HSI 16 MHz, SYSCLK 16 MHz.
7. Project Manager: Basic application structure, STM32CubeIDE.
8. Generate code.

---

## Build and Flash (STM32CubeIDE)

1. Open the generated project in STM32CubeIDE.
2. Copy `encoder_EC11.h` to `Inc/` and `encoder_EC11.c` to `Src/`.
3. Include the header in `main.c`.
4. Build the project.
5. Connect ST-Link and flash the firmware.

---

## Using the Encoder Driver

Typical usage:

1. Initialize the driver:
```c
EC11_Encoder_t encoder;
EC11_Init(&encoder);
```

2. Start the timer in encoder mode and reset it:
```c
HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
ENC_RESET();
encoder.lastTimerValue = ENC_READ();
```

3. In the main loop:
   - Read timer value
   - Compute `diff` using `EC11_TimerDiff16`
   - Pass `diff` to `EC11_ProcessTicks`
   - Process button via `EC11_ProcessButton`

Data available:
- `encoder.step` - logical position in detents
- `encoder.dir` - last rotation direction
- `encoder.buttonPressed` - button press flag

---

## Troubleshooting

- No reaction: Check TIM2 encoder mode and wiring.
- Direction reversed: Swap A/B or invert logic.
- Multiple steps per click: Adjust ticks-per-step or timer input filter.
- Button double triggers: Increase debounce time.
- SSD1306 not initializing: Check I2C pins, pull-ups, and clock speed.
- Large initial movement after reset: Initialize `lastTimerValue` after timer reset.

---

This project provides a clear starting point for working with rotary encoders and SSD1306 displays on STM32 and can be extended for menu navigation or other interactive applications.

---

## Small Example: LED On/Off Per Click

Goal: One click clockwise - LED ON, counter-clockwise - LED OFF

```c
int32_t lastStep = encoder.step;

while (1) {
   uint32_t now = HAL_GetTick();

   int32_t diff = EC11_TimerDiff16(&encoder, ENC_READ());
   if (diff != 0) {
      EC11_ProcessTicks(&encoder, diff);
   }

   EC11_ProcessButton(&encoder, (uint8_t)ENC_BTN_READ(), now, 50);

   if (encoder.step > lastStep) {
      LED_ON();
      lastStep = encoder.step;
   } else if (encoder.step < lastStep) {
      LED_OFF();
      lastStep = encoder.step;
   }
}
```

This ensures the code reacts to logical steps, not raw ticks.

---