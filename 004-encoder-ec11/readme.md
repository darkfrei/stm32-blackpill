# Project 4: Rotary Encoder EC11

This project demonstrates how to use an EC11 mechanical rotary encoder with an STM32 microcontroller (example: BlackPill STM32F411). The encoder rotation is decoded by a hardware timer in encoder interface mode, the push button is handled via GPIO with software debouncing. An SSD1306 (SH1106) OLED display is used for visualization.

---

## The project is about:

- `encoder_EC11.h` - public API and data structures of the EC11 driver
- `encoder_EC11.c` - driver implementation (tick accumulation, direction detection, button debounce)

**EC11**  
A common mechanical rotary encoder with two quadrature outputs (A and B) and an integrated push button.

**Quadrature signals (A/B)**  
Two square-wave signals shifted by 90 degrees. By comparing their phase, rotation direction can be detected.

**Tick**  
A single raw increment or decrement produced by the hardware encoder interface.

**Detent / step**  
A mechanical click position of the encoder. One step usually corresponds to several ticks (commonly 4).

**TIM (Timer)**  
A hardware timer peripheral in STM32. TIM2 is used here in encoder interface mode.

**Encoder interface mode**  
A special timer mode where the hardware decodes A/B signals and updates the counter automatically.

**GPIO**  
General Purpose Input Output pin, used here for the encoder push button and LED.

**HAL**  
Hardware Abstraction Layer provided by ST. It simplifies peripheral access.

**Debounce**  
Filtering technique to ignore rapid signal changes caused by mechanical contacts.

**Wraparound**  
When a timer counter reaches its maximum value and overflows back to zero. The driver compensates for this.

---

## Wiring and hardware notes

Typical connections for BlackPill STM32F411:

- Encoder A - `PA0` (TIM2_CH1)
- Encoder B - `PA1` (TIM2_CH2)
- Encoder button - `PA2` (GPIO input with pull-up)
- OLED SSD1306 - I2C1; example: `PB6` (SCL), `PB7` (SDA)
- Blue LED - `PC13`

Notes:
- Encoder A and B should have pull-ups (internal or external).
- The button is usually active-low (pressed = 0).
- Check your board schematic before wiring.

---

## STM32CubeMX configuration

1. Create a new project and select your MCU (for example STM32F411CEU6).
2. Graphical pins configuration:
  - `PC13` - GPIO Output (Blue LED)
Encoder:
  - Encoder A: `PA0` - TIM2_CH1 
  - Encoder B: `PA1` - TIM2_CH2
  - Encoder Button2 `PA2` - GPIO Input with Pull-Up
  Optional:
  - Encoder Button3 (Confirm) `PA3` - GPIO Input with Pull-Up
  - Encoder Button4 (Back) `PA4` - GPIO Input with Pull-Up
  Display: I2C pins for SSD1306 or SH1106
  - Clock SCL: `PB6` - I2C1_SCL
  - Data SDA: `PB7` - I2C1_SDA
  
3. Configure Timers: TIM2:
  - Slave Mode: Disable
  - Combined Channels: Encoder Mode
  - Prescaler: 0
  - Counter Mode: Up
  - Counter Periode: `65535`
  - Internal Clock: No Division
  - auto-reload preload: Enable
  - Encoder Mode: Encoder Mode TI1 and TI2
  - Ch1/Ch2 Polarity: Rising Edge
  - Ch1/Ch2 IC Selection: Direct
  
4. Configure System Core: GPIO: GPIO:
  - `PA2` - GPIO: Input Mode
  - `PA2` Pull-up/Pull-down: Pull-Up
  - Same for other buttons: `PA3`, `PA4`
5. Configure Connectivity: I2C1:
  - `PB6` I2C1_SCL, Pull-up/Pull-down: No Pull-Up and No Pull-down
  - `PB6` I2C1_SCL, Maximum output speed: Very high
  - Same for `PB7` I2C1_SDA
  
6. Clock Configuration:
  - HSI RC: 16 MHz
  - System Clock Mux: HSI
  - SYSCLK (MHz): 16
  - HCLK (MHz): 16

7. Project Manager:
  - Application Structure: Basic
  - Toolchain: STM32CubeIDE
  - Code Generator: Generate per. `.c` / `.h` files per peripheral

8. Generate code and open the project in CubeIDE.

---

## 5. STM32CubeIDE build and flash

1. Open the generated project in STM32CubeIDE.
2. Copy `encoder_EC11.h` into `Inc/` and `encoder_EC11.c` into `Src/`.
3. Include the header in `main.c`.
4. Build the project.
5. Connect ST-Link and flash the firmware.

---

## 6. Using the encoder driver

Typical usage pattern:

1. Initialize the driver:
  ```c
  EC11_Encoder_t encoder;
  EC11_Init(&encoder);
  ```

2. Start the hardware timer in encoder mode and reset it:
  ```c
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  ENC_RESET();
  encoder.lastTimerValue = ENC_READ();
  ```

3. In the main loop:
  - Read the timer value
  - Get `diff` from the driver
  - Pass `diff` to `EC11_ProcessTicks`
  - Process the button with `EC11_ProcessButton`

The application reads:
- `encoder.step` - logical position in detents
- `encoder.dir` - last direction
- `encoder.buttonPressed` - button press event flag

---

## 7. Example: LED on/off per click

Goal:
- One click clockwise - LED ON
- One click counter-clockwise - LED OFF

Example code:

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

This approach reacts to logical steps, not raw ticks.

---

## 8. Troubleshooting

- No reaction: check TIM2 encoder mode and wiring.
- Direction reversed: swap A/B signals or invert logic.
- Multiple steps per click: adjust ticks-per-step or timer input filter.
- Button double triggers: increase debounce time.
- Large first movement after reset: initialize `lastTimerValue` after resetting the timer.

---

This project is intended as a clean and understandable starting point for working with rotary encoders on STM32.

