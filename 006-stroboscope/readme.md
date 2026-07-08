# 006-stroboscope

STM32F411 BlackPill stroboscope with dual PWM control and OLED display.

---

## Features

- Strobe frequency: **0.15 Hz – 1000 Hz**, stored as millihertz  
  Linear steps: **0.1 / 1 / 10 Hz** per encoder click
- Flash duty cycle: **5–50%** (1% step) or **1/N** with N = 25–200 (step 5)
- LED brightness: **10–100%** (1% step) or **1/N** with N = 10–200 (step 1)
- Display always shows both percent and 1/N
- Dual PWM: TIM3 for strobe timing, TIM1 for brightness
- SH1106 OLED 128×64, three screens: frequency, duty, brightness
- EC11 rotary encoder with x1 / x10 / x100 multiplier
- Three buttons: step+save, screen cycle, strobe ON/OFF + reset
- Settings saved to Flash (sector 7), restored on boot
- Animated splash screen

---

## Hardware

### MCU

**STM32F411CEU6 (BlackPill)**  
100 MHz (HSE 25 MHz + PLL)

### Pin Assignment

| Pin  | Function               | Notes                                      |
|------|------------------------|--------------------------------------------|
| PA0  | TIM2_CH1 — Encoder A   | Quadrature input                           |
| PA1  | TIM2_CH2 — Encoder B   | Quadrature input                           |
| PA2  | BTN1 — EXTI2           | Short: step mult / Hold: save              |
| PA3  | BTN2 — EXTI3           | Short: cycle screens                       |
| PA4  | BTN3 — EXTI4           | Short: ON/OFF / Hold: reset                |
| PA8  | TIM1_CH1 — PWM         | LED brightness → MOSFET gate               |
| PB0  | GPIO Output            | Debug strobe mirror                        |
| PB6  | I2C1_SCL               | OLED SH1106                                |
| PB7  | I2C1_SDA               | OLED SH1106                                |
| PC13 | GPIO Output            | Onboard LED (active LOW)                   |

### LED Wiring

~~~
PA8 ---- MOSFET IN
GND ---- MOSFET GND

MOSFET OUT ---- LED+ ---- LED ---- GND
Power (+) ---- MOSFET VCC
~~~

MOSFET: AOD4184 module, 3.3V logic OK.  
Gate resistor 56 Ω + 100 kΩ pull‑down onboard.

### Buttons

All buttons: pin → GND  
Internal pull‑up, debounce 80 ms  
Long press: 800 ms

---

## Timer Architecture

### Clock Tree

~~~
HSE 25 MHz → PLL (12, 96, /2) → SYSCLK 100 MHz
APB1 timers: 100 MHz (TIM2, TIM3)
APB2 timers: 100 MHz (TIM1)
~~~

### TIM1 — LED Brightness PWM (PA8)

~~~
PSC = 9   → 10 MHz
ARR = 999 → 10 kHz PWM
~~~

Percent mode:
~~~
CCR1 = (ARR+1) × % / 100
~~~

Divisor mode:
~~~
CCR1 = (ARR+1) / N
~~~

TIM1 CCR1 updated from TIM3 ISR.

### TIM2 — EC11 Encoder

~~~
Encoder mode TI1+TI2
ARR = 65535
Overflow‑safe delta computation
~~~

### TIM3 — Strobe Timer (interrupt only)

Frequency stored as **millihertz**:

~~~
period_ticks = 10,000,000 / freq_mhz
PSC = 9999 → 10 kHz tick (0.1 ms)
ARR = period_ticks − 1
~~~

Duty:

~~~
Percent: CCR1 = (ARR+1) × % / 100
Divisor: CCR1 = (ARR+1) / N
~~~

#### Interrupt Logic

~~~
TIM3 Update:
  TIM1 CCR1 = Brig_GetCCR()   // LED ON
  PB0 = HIGH, PC13 = LOW
  Enable CC1 interrupt

TIM3 CC1:
  TIM1 CCR1 = 0               // LED OFF
  PB0 = LOW, PC13 = HIGH
  Disable CC1 interrupt
~~~

#### Apply Strategy

| Change | Action                                             |
|--------|-----------------------------------------------------|
| Freq   | Stop, force OFF, set ARR+CCR, restart at ARR        |
| Duty   | Update CCR only                                     |
| Bright | Read live in ISR                                    |

---

## Parameter Ranges

### Frequency

| Range           | Step     | Display     |
|-----------------|----------|-------------|
| 0.15–0.99 Hz    | 0.1 Hz   | `0.15Hz`    |
| 1.0–9.99 Hz     | 0.1/1 Hz | `1.50Hz`    |
| 10–99.9 Hz      | 1 Hz     | `30.0Hz`    |
| 100–1000 Hz     | 10 Hz    | `100Hz`     |

Step multiplier:

| Mult | Step per click |
|------|-----------------|
| x1   | 0.1 Hz          |
| x10  | 1 Hz            |
| x100 | 10 Hz           |

### Duty Cycle

| Mode         | Range      | Step | Display        |
|--------------|------------|------|----------------|
| Percent      | 5–50%      | 1%   | `12%  1/8`     |
| Divisor      | 1/25–1/200 | +5   | `2%   1/50`    |

Boundary: 5% = 1/20.

### Brightness

| Mode         | Range      | Step | Display        |
|--------------|------------|------|----------------|
| Percent      | 10–100%    | 1%   | `75%  1/1`     |
| Divisor      | 1/10–1/200 | +1   | `5%   1/20`    |

Boundary: 10% = 1/10.

---

## Display Layout (SH1106 128×64)

Rows 1–8 partially broken → content starts at y=11.

### Main Screen

~~~
STEP 0.1 Hz
          3 0 . 0 H z
[ ON ] BTN3=off
~~~

### Duty / Brightness Screens

~~~
DUTY CYCLE
  5%   1/20
  STEP 1
BTN2: next
[ ON ] BTN3=off
~~~

### Big Digit Dimensions

| Constant  | Value | Meaning                  |
|-----------|-------|--------------------------|
| SEG_W     | 16 px | digit width              |
| SEG_H     | 28 px | digit height             |
| SEG_GAP   | 2 px  | gap between digits       |
| SEG_DOT_W | 4 px  | decimal point width      |

---

## Controls

| Control                     | Action                                     |
|-----------------------------|---------------------------------------------|
| Encoder CW/CCW             | Adjust active parameter                     |
| BTN1 short                 | Cycle step multiplier                       |
| BTN1 hold                  | Save to Flash                               |
| BTN2 short                 | Cycle screens                               |
| BTN3 short                 | Strobe ON/OFF                               |
| BTN3 hold                  | Reset to defaults                           |

---

## Default Parameters

| Parameter  | Default   | Range              |
|------------|-----------|--------------------|
| Frequency  | 30.0 Hz   | 0.15–1000 Hz       |
| Duty       | 5%        | 5–50% / 1/25–1/200 |
| Brightness | 75%       | 10–100% / 1/10–200 |
| Step mult  | x1        | x1 / x10 / x100    |

---

## Flash Storage

Sector 7 — `0x08060000` (128 KB)

| Event        | Action                                      |
|--------------|----------------------------------------------|
| BTN1 hold    | Erase + write all parameters                 |
| Power‑on     | Load if magic + checksum OK                  |
| BTN3 hold    | Reset RAM only (Flash untouched)             |

Magic: `0x5752B008`  
Fields: freq_mhz, step_idx, duty mode/value, brightness mode/value, running state.

---

## Project Structure

~~~
006-stroboscope/
├── 006-stroboscope.ioc
├── README.md
├── App/
│   ├── SH1106/
│   └── EC11/
└── Core/
    ├── Inc/
    └── Src/
~~~

---

## CubeMX Configuration Summary

| Peripheral | Setting                                             |
|------------|-----------------------------------------------------|
| RCC        | HSE 25 MHz, PLL → 100 MHz                           |
| TIM1 CH1   | PWM, PSC=9, ARR=999                                 |
| TIM2       | Encoder TI1+TI2, ARR=65535                          |
| TIM3 CH1   | OC, PSC=9999, ARR=332, Pulse=16                     |
| TIM3 NVIC  | Enabled, priority 2                                 |
| I2C1       | Fast Mode 400 kHz                                   |
| PA2–PA4    | EXTI Falling, Pull‑up, prio 5                       |
| PB0        | GPIO Output High Speed                              |
| PC13       | GPIO Output                                         |

---

## Build Notes

### Include App folders

~~~
App/SH1106 → uncheck "Exclude from build"
App/EC11   → uncheck "Exclude from build"
~~~

### TIM3_IRQHandler

Defined in `main.c`.  
Comment out CubeMX version:

~~~
void TIM3_IRQHandler(void)
{
    /* defined in main.c */
    // HAL_TIM_IRQHandler(&htim3);
}
~~~

### SystemClock_Config / Error_Handler

Defined in `main.c`.  
If CubeMX regenerates duplicates — remove from `main.c`.
