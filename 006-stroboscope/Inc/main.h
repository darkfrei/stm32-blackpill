/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for 006-stroboscope project
 *
 * STM32F411 BlackPill — Stroboscope
 *
 * Pins:
 *   PA0  — TIM2_CH1  (Encoder A)
 *   PA1  — TIM2_CH2  (Encoder B)
 *   PA2  — EXTI2     (BTN1 — parameter select)
 *   PA3  — EXTI3     (BTN2 — strobe on/off)
 *   PA4  — EXTI4     (BTN3 — reset to default)
 *   PA8  — TIM1_CH1  (PWM brightness → external LED)
 *   PB0  — GPIO Out  (LED ext, duplicates strobe for debugging)
 *   PB6  — I2C1_SCL  (OLED SSD1306)
 *   PB7  — I2C1_SDA  (OLED SSD1306)
 *   PC13 — GPIO Out  (Onboard LED — operation indicator)
 *
 * Clock: HSE 25 MHz → PLL → 100 MHz
 *   APB1 Timer clock : 100 MHz
 *   APB2 Timer clock : 100 MHz
 *
 * TIM1 (APB2, 100 MHz) — LED brightness PWM:
 *   Prescaler = 9  → f_cnt = 10 MHz
 *   ARR = 999      → f_pwm = 10 kHz
 *   CCR1  = 0..999 → brightness duty
 *
 * TIM2 (APB1, 100 MHz) — quadrature encoder:
 *   ARR = 65535, EncoderMode TI12
 *
 * TIM3 (APB1, 100 MHz) — strobe timer:
 *   Prescaler = 9999 → f_cnt = 10 kHz (tick = 0.1 ms)
 *   ARR  = (10000 / freq) - 1  → strobe period
 *   CCR1 = ARR * duty% / 100   → LED off moment
 *   Update interrupt → LED ON  (set TIM1 CCR1 for brightness)
 *   CC1 interrupt     → LED OFF (TIM1 CCR1 = 0)
 *
 * Control:
 *   Encoder  — change selected parameter
 *   BTN1     — change parameter (Freq / Duty / Bright)
 *   BTN2     — strobe on/off
 *   BTN3     — reset to initial values
 ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* ── GPIO Labels ─────────────────────────────────────────────────────────── */
#define LED_ONBOARD_Pin          GPIO_PIN_13
#define LED_ONBOARD_GPIO_Port    GPIOC

#define LED_EXT_Pin              GPIO_PIN_0
#define LED_EXT_GPIO_Port        GPIOB

/* BTN — active LOW, internal pull-up to VCC */
#define BTN1_Pin                 GPIO_PIN_2          /* parameter select      */
#define BTN1_GPIO_Port           GPIOA
#define BTN1_EXTI_IRQn           EXTI2_IRQn

#define BTN2_Pin                 GPIO_PIN_3          /* strobe on/off         */
#define BTN2_GPIO_Port           GPIOA
#define BTN2_EXTI_IRQn           EXTI3_IRQn

#define BTN3_Pin                 GPIO_PIN_4          /* reset defaults        */
#define BTN3_GPIO_Port           GPIOA
#define BTN3_EXTI_IRQn           EXTI4_IRQn

/* ── Strobe settings ─────────────────────────────────────────────────────── */
#define STROBE_FREQ_MIN          10u     /* Hz  */
#define STROBE_FREQ_MAX          100u    /* Hz  */
#define STROBE_FREQ_INIT         30u     /* Hz  */

#define STROBE_DUTY_MIN          1u      /* %   */
#define STROBE_DUTY_MAX          50u     /* %   */
#define STROBE_DUTY_INIT         2u      /* % ≈ 1/50 */

#define STROBE_BRIGHT_MIN        10u     /* %   */
#define STROBE_BRIGHT_MAX        100u    /* %   */
#define STROBE_BRIGHT_INIT       75u     /* %   */

/* ── TIM1 PWM parameters ─────────────────────────────────────────────────── */
#define TIM1_PWM_PERIOD          999u    /* ARR: 10 kHz brightness PWM */

/* ── TIM3 strobe parameters ───────────────────────────────────────────────── */
#define TIM3_TICK_FREQ           10000u  /* Hz after prescaler (PSC=9999) */

/* ── Button debounce ─────────────────────────────────────────────────────── */
#define BTN_DEBOUNCE_MS          80u

/* ── Display refresh period ─────────────────────────────────────────────── */
#define DISPLAY_REFRESH_MS       100u

/* ── Error handler ───────────────────────────────────────────────────────── */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif
#endif /* __MAIN_H */