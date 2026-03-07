/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.h
  * @brief   006-stroboscope — pin definitions and constants
  *
  * Pins:
  *   PA0  — TIM2_CH1  (Encoder A)
  *   PA1  — TIM2_CH2  (Encoder B)
  *   PA2  — EXTI2     (BTN1 — parameter select)
  *   PA3  — EXTI3     (BTN2 — strobe on/off)
  *   PA4  — EXTI4     (BTN3 — reset to defaults)
  *   PA8  — TIM1_CH1  (PWM brightness → MOSFET gate)
  *   PB0  — GPIO Out  (debug strobe mirror)
  *   PB6  — I2C1_SCL  (OLED SH1106)
  *   PB7  — I2C1_SDA  (OLED SH1106)
  *   PC13 — GPIO Out  (onboard LED, active LOW)
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* ── GPIO labels ─────────────────────────────────────────────────────────── */
#define LED_ONBOARD_Pin         GPIO_PIN_13
#define LED_ONBOARD_GPIO_Port   GPIOC

#define LED_EXT_Pin             GPIO_PIN_0
#define LED_EXT_GPIO_Port       GPIOB

/* Buttons — active LOW, internal pull-up */
#define BTN1_Pin                GPIO_PIN_2      /* parameter select  */
#define BTN1_GPIO_Port          GPIOA
#define BTN1_EXTI_IRQn          EXTI2_IRQn

#define BTN2_Pin                GPIO_PIN_3      /* strobe on/off     */
#define BTN2_GPIO_Port          GPIOA
#define BTN2_EXTI_IRQn          EXTI3_IRQn

#define BTN3_Pin                GPIO_PIN_4      /* reset to defaults */
#define BTN3_GPIO_Port          GPIOA
#define BTN3_EXTI_IRQn          EXTI4_IRQn

/* ── Button debounce ─────────────────────────────────────────────────────── */
#define BTN_DEBOUNCE_MS         80u

/* ── Display refresh period ──────────────────────────────────────────────── */
#define DISPLAY_REFRESH_MS      100u

/* ── Error handler ───────────────────────────────────────────────────────── */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif
#endif /* __MAIN_H */