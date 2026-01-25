# BlackPill STM32F411CE — LED Breathing (Software PWM)

This repository contains a minimal example that implements a smooth "breathing" LED effect on the WeAct BlackPill STM32F411CEU6 board using software PWM (bit-banging). The implementation uses the DWT cycle counter for microsecond-accurate delays and the HAL GPIO API to toggle the onboard LED on PC13 (inverted logic).

---

## Hardware

- **Board:** WeAct STM32F411CEU6 (BlackPill)
- **MCU:** STM32F411CEU6 (Cortex‑M4)
- **Clock source:** Internal HSI
- **Programmer:** ST‑Link V2
- **LED pin:** PC13 (onboard LED, inverted polarity)

---

## What this project does

- Produces a continuous breathing (fade in / fade out) animation on the onboard LED.
- Generates PWM in software with a configurable resolution and base period.
- Uses DWT cycle counter for accurate `delay_us()` implementation.
- No hardware timers or interrupts are required.

The breathing sequence repeatedly increases PWM duty from 0% to 100% and then decreases it back to 0%, with a short pause between duty steps to control the breathing speed.

---

## Key parameters (in `main.c`)

```c
#define PWM_PERIOD     100     // PWM resolution (0..100)
#define PWM_DELAY_US   10      // PWM base delay (10 us -> ~1 kHz PWM)
#define FADE_DELAY_MS  20      // Delay between duty steps, controls breathing speed
```

- Increase `PWM_PERIOD` to get smoother brightness steps.
- Increase `PWM_DELAY_US` or `PWM_PERIOD` to change PWM frequency (avoid visible flicker).
- Adjust `FADE_DELAY_MS` to speed up or slow down the breathing effect.

---

## How it works (short)

1. `dwt_init()` enables the DWT cycle counter.
2. `delay_us()` uses `DWT->CYCCNT` to implement microsecond delays.
3. For each duty step, code toggles PC13 for `PWM_PERIOD` sub-intervals and uses `delay_us(PWM_DELAY_US)` to control the PWM period.
4. After each duty step the code waits `FADE_DELAY_MS` milliseconds to control the breathing speed.

Note: On the BlackPill, PC13 is wired so that `GPIO_PIN_RESET` turns the LED on and `GPIO_PIN_SET` turns it off. The code uses this inverted logic explicitly.

---

## Build and flash

Requirements:
- STM32CubeMX 6.x (project generated with CubeMX)
- STM32CubeIDE (or a compatible arm-none-eabi toolchain)
- ST‑Link V2 and drivers

Steps (STM32CubeIDE):
1. Import or open the project folder containing the `.ioc` file.
2. Build the project (Ctrl+B).
3. Connect the board with ST‑Link and flash (Ctrl+F11) or debug (F11).

Cleaning build artifacts:
- The `Debug/` directory contains build artifacts and can be deleted safely or cleaned via Project → Clean.

---

## Expected result

After flashing the firmware the onboard LED on **PC13** performs a continuous, smooth breathing animation without visible flicker.

---

## File layout (relevant parts)

```
002-breath/
├── Core/
│   ├── Inc/
│   └── Src/
│       ├── main.c            # application code with breathing implementation
│       ├── stm32f4xx_it.c
│       ├── stm32f4xx_hal_msp.c
│       └── system_stm32f4xx.c
├── Drivers/
├── 002-breath.ioc           # CubeMX configuration
├── STM32F411CEUX_FLASH.ld
└── Makefile
```

---

## Limitations and notes

- Software PWM is CPU‑blocking. This example is intended for simple demos and learning.
- For multitasking or low-power designs use hardware timers (TIM) with PWM output and DMA if needed.
- PC13 is slower than regular GPIO pins but is adequate for LED control.
- Ensure CMSIS device headers are included so `DWT` and `CoreDebug` symbols are available.

---

## License

Project code is provided as-is for educational purposes. The ST HAL drivers used in this project are distributed under the BSD‑3‑Clause license.

---

## Author

darkfrei

---

## Version

- v1.0.0 (2026-01-11) — Software PWM breathing LED on PC13 using DWT timing

