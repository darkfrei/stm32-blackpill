# 006-stroboscope

STM32F411 BlackPill stroboscope with dual PWM control and OLED display.

---

## Features

- Strobe frequency: **10–100 Hz** (step 1 Hz) or **150–6500 ms** period (step 50 ms)
- Flash duty cycle: **5–50%** (step 1%) or **1/N** divisor N = 25–200 (step 5)
- LED brightness: **10–100%** (step 1%) or **1/N** divisor N = 10–200 (step 1)
- Display always shows both percent and 1/N for duty and brightness
- Dual PWM: separate timers for strobe timing (TIM3) and brightness (TIM1)
- 128×64 OLED (SH1106, I2C) showing all parameters live
- Rotary encoder EC11 for parameter adjustment
- Three buttons: parameter cycle + save, strobe ON/OFF, reset to defaults
- Settings saved to Flash (sector 7) on encoder button press, restored on power-on

---

## Hardware

### MCU
STM32F411CEU6 (BlackPill), 100 MHz (HSE 25 MHz + PLL)

### Pin Assignment

| Pin  | Function             | Notes                             |
|------|----------------------|-----------------------------------|
| PA0  | TIM2_CH1 — Encoder A | Quadrature input                  |
| PA1  | TIM2_CH2 — Encoder B | Quadrature input                  |
| PA2  | BTN1 — EXTI2         | Cycle parameter + save (active LOW) |
| PA3  | BTN2 — EXTI3         | Strobe ON/OFF (active LOW)        |
| PA4  | BTN3 — EXTI4         | Reset to defaults (active LOW)    |
| PA8  | TIM1_CH1 — PWM       | LED brightness → MOSFET gate      |
| PB0  | GPIO Output          | Debug strobe mirror (GPIO toggle) |
| PB6  | I2C1_SCL             | OLED SH1106                       |
| PB7  | I2C1_SDA             | OLED SH1106                       |
| PC13 | GPIO Output          | Onboard LED (active LOW)          |

### LED Wiring

```
PA8 ---- MOSFET module IN
GND ---- MOSFET module GND

MOSFET module OUT ---- LED+ ---- LED ---- GND
Power supply (+)  ---- MOSFET module VCC
```

The MOSFET module (AO3400A-based) accepts 3.3V logic directly from PA8 —
no additional resistor needed between PA8 and the module gate input.

### Buttons

All buttons wired between pin and GND.
Internal pull-up enabled (GPIO_PULLUP), software debounce 80 ms.

---

## Timer Architecture

### Clock Tree

```
HSE 25 MHz → PLL (PLLM=12, PLLN=96, PLLP=2) → SYSCLK 100 MHz
  APB1 timer clock : 100 MHz  (TIM2, TIM3)
  APB2 timer clock : 100 MHz  (TIM1)
```

### TIM1 — LED Brightness PWM (PA8)

Controls LED brightness via high-frequency PWM on the MOSFET gate.

```
Prescaler = 9   → f_count = 10 MHz
ARR       = 999 → f_PWM   = 10 kHz

Percent mode : CCR1 = (ARR+1) × brightness% / 100
Divisor mode : CCR1 = (ARR+1) / N
  N=10  → CCR1 = 100  (10%)
  N=100 → CCR1 = 10   (1%)
  N=200 → CCR1 = 5    (0.5%)
```

CCR1 is written directly from the TIM3 Update ISR. No TIM1 interrupts are used.

### TIM2 — Quadrature Encoder EC11 (PA0 / PA1)

```
Encoder Mode TI1 and TI2
ARR = 65535
Counter read as int16_t → signed delta per main loop iteration
```

### TIM3 — Strobe Timer (interrupts only, no output pin)

```
Prescaler = 9999 → f_count = 10 kHz (tick = 0.1 ms)

Hz mode    : ARR = (10000 / freq_Hz) − 1
Period mode: ARR = (period_ms × 10) − 1   (max 6500 ms → ARR = 64999)

CCR1 (duty):
  Percent mode : CCR1 = (ARR+1) × duty% / 100
  Divisor mode : CCR1 = (ARR+1) / N

Examples at duty = 5% (1/20):
  10 Hz   → ARR = 999,   CCR1 = 50   → flash = 5.0 ms
  30 Hz   → ARR = 332,   CCR1 = 16   → flash = 1.6 ms
  100 Hz  → ARR = 99,    CCR1 = 5    → flash = 0.5 ms
  500 ms  → ARR = 4999,  CCR1 = 250  → flash = 25 ms
```

#### Interrupt Logic

```
TIM3 Update (every period)
  → TIM1 CCR1 = Brig_GetCCR()   LED turns ON
  → PB0 = HIGH, PC13 = LOW
  → Enable CC1 interrupt

TIM3 CC1 (duty point reached)
  → TIM1 CCR1 = 0               LED turns OFF
  → PB0 = LOW, PC13 = HIGH
  → Disable CC1 interrupt until next Update
```

#### Apply strategy (no TIM3 counter reset unless frequency changes)

| Change   | Action                              |
|----------|-------------------------------------|
| Freq     | Full stop → set ARR+CCR → restart   |
| Duty     | Set CCR only — no counter reset     |
| Bright   | Read live in ISR — nothing to apply |

---

## Parameter Ranges

### Frequency

| Mode        | Range          | Step   | Display example      |
|-------------|----------------|--------|----------------------|
| Hz mode     | 10–100 Hz      | 1 Hz   | `Freq:  30 Hz`       |
| Period mode | 150–6500 ms    | 50 ms  | `500ms 2.0Hz`        |
|             |                |        | `1.50s 0.66Hz`       |

Crossing: turning CCW below 10 Hz switches to period mode at 150 ms; turning CW
above 150 ms switches back to Hz mode at 10 Hz.

### Duty cycle

| Mode         | Range       | Step | Display example  |
|--------------|-------------|------|------------------|
| Percent mode | 5–50%       | 1%   | `Duty:12% 1/8`   |
| Divisor mode | 1/25–1/200  | N+5  | `Duty: 2% 1/50`  |

Boundary at 5% = 1/20.

### Brightness

| Mode         | Range       | Step | Display example  |
|--------------|-------------|------|------------------|
| Percent mode | 10–100%     | 1%   | `Brig:75% 1/1`   |
| Divisor mode | 1/10–1/200  | N+1  | `Brig: 5% 1/20`  |

Boundary at 10% = 1/10. CCR calculated directly from N (not via percent),
preserving resolution at low brightness (e.g. 1/100 ≠ 1/200).

---

## Display Layout (SH1106 128×64)

```
┌------------------------┐
│   = STROBE 006 =       │   -- inverted header
│ > Freq:  30 Hz         │   -- '>' marks editable parameter
│   Duty:  5% 1/20       │
│   Brig: 75% 1/1        │
│  [ ON ]  BTN2=off      │   -- status bar (inverted)
└------------------------┘
```

After saving, the status bar briefly shows `SAVED` for 600 ms.

---

## Controls

| Control          | Action                                    |
|------------------|-------------------------------------------|
| Encoder CW       | Increase selected parameter               |
| Encoder CCW      | Decrease selected parameter               |
| BTN1 / ENC push (PA2) | Cycle Freq → Duty → Bright + save to Flash |
| BTN2 (PA3)       | Toggle strobe ON / OFF                    |
| BTN3 (PA4)       | Reset all parameters to defaults          |

---

## Default Parameters

| Parameter  | Default      | Range                   |
|------------|--------------|-------------------------|
| Frequency  | 30 Hz        | 10–100 Hz / 150–6500 ms |
| Duty cycle | 5% (1/20)    | 5–50% / 1/25–1/200      |
| Brightness | 75% (1/1)    | 10–100% / 1/10–1/200    |

---

## Flash Storage

Settings are saved to **Flash sector 7** (address `0x08060000`, 128 KB).

| Event              | Action                                 |
|--------------------|----------------------------------------|
| BTN1 / encoder push | Erase sector 7, write all parameters  |
| Power-on           | Load from Flash if magic + checksum OK |
| BTN3 (reset)       | Restores defaults in RAM only — does not erase Flash |

Stored fields: frequency mode/value, duty mode/value, brightness mode/value,
running state. Protected by XOR checksum and a magic word (`0x5752B006`).

---

## Project Structure

```
006-stroboscope/
├-- 006-stroboscope.ioc
├-- README.md
├-- App/
│   ├-- SH1106/        -- OLED driver (sh1106.c/h, sh1106_fonts.c/h)
│   └-- EC11/          -- Encoder driver (EC11.c/h)
├-- Inc/
│   ├-- main.h
│   ├-- gpio.h
│   ├-- i2c.h
│   ├-- spi.h
│   ├-- stm32f4xx_hal_conf.h
│   ├-- stm32f4xx_it.h
│   └-- tim.h
└-- Src/
    ├-- gpio.c
    ├-- i2c.c
    ├-- main.c         -- all application logic + TIM3_IRQHandler
    ├-- spi.c
    ├-- stm32f4xx_hal_msp.c
    ├-- stm32f4xx_it.c
    ├-- syscalls.c
    ├-- sysmem.c
    ├-- system_stm32f4xx.c
    └-- tim.c
```

---

## CubeMX Configuration Summary

| Peripheral | Setting                                                |
|------------|--------------------------------------------------------|
| RCC        | HSE Crystal 25 MHz, PLL → 100 MHz                     |
| TIM1 CH1   | PWM Generation, PSC=9, ARR=999, Pulse=0               |
| TIM2       | Encoder Mode TI1+TI2, ARR=65535                       |
| TIM3 CH1   | Output Compare No Output, PSC=9999, ARR=332, Pulse=6  |
| TIM3 NVIC  | TIM3 global interrupt enabled, priority 2             |
| I2C1       | Fast Mode 400 kHz (PB6 SCL / PB7 SDA)                |
| PA2–PA4    | EXTI Falling Edge, Pull-up, priority 5                |
| PB0        | GPIO Output High Speed (debug strobe mirror)          |
| PC13       | GPIO Output (onboard LED, active LOW)                 |

---

## Build Notes

### App folders must be included in build

In STM32CubeIDE, right-click each App subfolder and verify it is
not excluded from build:

```
App/SH1106  → Properties → C/C++ Build → uncheck "Exclude from build"
App/EC11    → Properties → C/C++ Build → uncheck "Exclude from build"
```

### TIM3_IRQHandler conflict

`TIM3_IRQHandler` is defined in `main.c`.
Open `Src/stm32f4xx_it.c` and comment out the generated body:

```c
void TIM3_IRQHandler(void)
{
    /* defined in main.c */
    // HAL_TIM_IRQHandler(&htim3);
}
```

### SystemClock_Config and Error_Handler

Both functions are defined at the bottom of `main.c`.
If CubeMX regenerates them in its own skeleton, remove the copies from `main.c`
to avoid duplicate symbol errors.
