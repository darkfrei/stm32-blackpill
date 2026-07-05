/* Core/Inc/main.h */

#ifndef __MAIN_H
#define __MAIN_H
#ifdef __cplusplus
extern "C" {
#endif

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.h
  * @brief   006-stroboscope -- pin definitions and constants
  *
  * Pins:
  *   PA0  -- TIM2_CH1  (encoder A)
  *   PA1  -- TIM2_CH2  (encoder B)
  *   PA2  -- EXTI2     (encoder push -- cycle step / hold = save)
  *   PA3  -- EXTI3     (bottom button -- cycle screens)
  *   PA4  -- EXTI4     (top button -- strobe on/off / hold = reset)
  *   PA8  -- TIM1_CH1  (PWM brightness -> MOSFET gate)
  *   PB0  -- GPIO out  (debug strobe mirror)
  *   PB6  -- I2C1_SCL  (OLED SH1106)
  *   PB7  -- I2C1_SDA  (OLED SH1106)
  *   PC13 -- GPIO out  (onboard LED, active LOW)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "stm32f4xx_hal.h"

/* GPIO labels */
#define LED_ONBOARD_Pin         GPIO_PIN_13
#define LED_ONBOARD_GPIO_Port   GPIOC
#define LED_EXT_Pin             GPIO_PIN_0
#define LED_EXT_GPIO_Port       GPIOB

/* buttons -- active LOW, internal pull-up */
#define BTN1_Pin            GPIO_PIN_2  /* encoder push -- cycle step / hold = save    */
#define BTN1_GPIO_Port      GPIOA
#define BTN1_EXTI_IRQn      EXTI2_IRQn
#define BTN2_Pin            GPIO_PIN_3  /* bottom button -- cycle screens              */
#define BTN2_GPIO_Port      GPIOA
#define BTN2_EXTI_IRQn      EXTI3_IRQn
#define BTN3_Pin            GPIO_PIN_4  /* top button -- strobe on/off / hold = reset  */
#define BTN3_GPIO_Port      GPIOA
#define BTN3_EXTI_IRQn      EXTI4_IRQn

/* button timing */
#define BTN_DEBOUNCE_MS     80u
#define BTN_HOLD_MS        800u

/* display refresh period */
#define DISPLAY_REFRESH_MS  100u

void Error_Handler(void);

#ifdef __cplusplus
}
#endif
#endif /* __MAIN_H */
