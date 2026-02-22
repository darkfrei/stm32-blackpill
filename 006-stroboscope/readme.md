# 006-stroboscope

STM32F411 BlackPill stroboscope with dual PWM control and OLED display.

---

## Features

- Strobe frequency: **10–100 Hz**, adjustable in real time
- Flash duty cycle: **1–50%**, default **2% (1/50)**
- LED brightness: **10–100%**, default **75%**
- Dual PWM: separate timers for strobe timing and brightness
- 128×64 OLED (SH1106, I2C) showing all parameters
- Rotary encoder EC11 for parameter adjustment
- Three buttons: parameter select, strobe ON/OFF, reset to defaults

---

## Hardware

### MCU
STM32F411CEU6 (BlackPill), 100 MHz (HSE 25 MHz + PLL)

### Pin Assignment

| Pin  | Function             | Notes                             |
|------|----------------------|-----------------------------------|
| PA0  | TIM2_CH1 — Encoder A | Quadrature input                  |
| PA1  | TIM2_CH2 — Encoder B | Quadrature input                  |
| PA2  | BTN1 — EXTI2         | Parameter select (active LOW)     |
| PA3  | BTN2 — EXTI3         | Strobe ON/OFF (active LOW)        |
| PA4  | BTN3 — EXTI4         | Reset to defaults (active LOW)    |
| PA8  | TIM1_CH1 — PWM       | LED brightness → MOSFET gate      |
| PB0  | GPIO Output          | Debug strobe mirror (GPIO toggle) |
| PB6  | I2C1_SCL             | OLED SH1106                       |
| PB7  | I2C1_SDA             | OLED SH1106                       |
| PC13 | GPIO Output          | Onboard LED (active LOW)          |

### LED Wiring

```
PA8 ──── MOSFET module IN
GND ──── MOSFET module GND

MOSFET module OUT ──── LED+ ──── LED ──── GND
Power supply (+)  ──── MOSFET module VCC
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

CCR1 = (ARR + 1) × brightness% / 100
  0%   → CCR1 = 0
  75%  → CCR1 = 750
  100% → CCR1 = 1000
```

CCR1 is written directly from the TIM3 ISR. No TIM1 interrupts are used.

### TIM2 — Quadrature Encoder EC11 (PA0 / PA1)

```
Encoder Mode TI1 and TI2
ARR = 65535
Counter read as int16_t → signed delta per main loop iteration
```

### TIM3 — Strobe Timer (interrupts only, no output pin)

```
Prescaler = 9999 → f_count = 10 kHz (tick = 0.1 ms)
ARR  = (10000 / freq_Hz) − 1
CCR1 = (ARR + 1) × duty% / 100

Examples at duty = 2%:
  10 Hz  → ARR = 999,  CCR1 = 20  → flash = 2.0 ms
  30 Hz  → ARR = 332,  CCR1 = 6   → flash = 0.6 ms
  100 Hz → ARR = 99,   CCR1 = 2   → flash = 0.2 ms
```

#### Interrupt Logic

```
TIM3 Update (every period)
  → TIM1 CCR1 = Bright_CCR()   LED turns ON
  → PB0 = HIGH, PC13 = LOW
  → Enable CC1 interrupt

TIM3 CC1 (duty point reached)
  → TIM1 CCR1 = 0              LED turns OFF
  → PB0 = LOW, PC13 = HIGH
  → Disable CC1 interrupt until next Update
```

---

## Display Layout (SH1106 128×64)

```
┌────────────────────────┐
│   = STROBE 006 =       │
│ > Freq:  30 Hz         │  ← '>' marks editable parameter
│   Duty:   2% 1/50      │
│   Brig:  75%           │
│  [ ON ]  BTN2=off      │
└────────────────────────┘
```

---

## Controls

| Control     | Action                              |
|-------------|-------------------------------------|
| Encoder CW  | Increase selected parameter         |
| Encoder CCW | Decrease selected parameter         |
| BTN1 (PA2)  | Cycle: Freq → Duty → Bright → Freq  |
| BTN2 (PA3)  | Toggle strobe ON / OFF              |
| BTN3 (PA4)  | Reset all parameters to defaults    |

---

## Default Parameters

| Parameter  | Default   | Range     | Step |
|------------|-----------|-----------|------|
| Frequency  | 30 Hz     | 10–100 Hz | 1 Hz |
| Duty cycle | 2% (1/50) | 1–50%     | 1%   |
| Brightness | 75%       | 10–100%   | 1%   |

---

## Project Structure

```
006-stroboscope/
├── 006-stroboscope.ioc
├── README.md
├── App/
│   ├── SH1106/        ← OLED driver (sh1106.c/h, sh1106_fonts.c/h)
│   └── EC11/          ← Encoder driver (EC11.c/h)
├── Inc/
│   └── main.h
└── Src/
    ├── main.c         ← all application logic + TIM3_IRQHandler
    ├── gpio.c
    ├── i2c.c
    ├── tim.c
    ├── stm32f4xx_it.c
    └── stm32f4xx_hal_msp.c
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