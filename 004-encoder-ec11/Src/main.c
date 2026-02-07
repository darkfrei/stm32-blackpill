/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : EC11 Encoder Test
  ******************************************************************************
  * @attention
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    int32_t step;            /* logical step counter */
    int32_t counter;         /* tick accumulator (-4..+4) */
    uint8_t direction;       /* 0 = none, 1 = cw, 2 = ccw */
    uint8_t button_state;    /* current debounced button state */
    uint8_t button_pressed;  /* button press event flag */
} Encoder_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UPDATE_DELAY_MS 200
#define ENCODER_TICKS_PER_STEP 4    /* 4 ticks per mechanical detent */
#define ENCODER_MAX_TICKS 4
#define ENCODER_MIN_TICKS -4
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define ENC_READ()       ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim2)))
#define ENC_RESET()      (__HAL_TIM_SET_COUNTER(&htim2, 0))
#define ENC_BTN_READ()   HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2)
#define LED_TOGGLE()     HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
Encoder_t encoder = {0};

uint32_t last_update = 0;
uint32_t ups_counter = 0;
uint32_t ups_value = 0;
uint32_t last_ups_time = 0;

uint8_t display_dirty = 1;

char display_buf[48];
char temp_buf[16];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Display_Init(void);
void Display_Update(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
  * @brief  application entry point
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* reset peripherals, init flash and systick */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* configure system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* initialize configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();

  /* USER CODE BEGIN 2 */
  /* small delay for power and display startup */
  HAL_Delay(50);

  /* initialize oled */
  if (ssd1306_Init() != SSD1306_OK) {
      /* blink led forever on oled init failure */
      while (1) {
          LED_TOGGLE();
          HAL_Delay(200);
      }
  }

  ssd1306_Fill(Black);
  ssd1306_UpdateScreen();

  /* start encoder timer */
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  ENC_RESET();

  static uint16_t last_encoder_value = 0;
  last_encoder_value = ENC_READ();

  /* initialize encoder state */
  encoder.step = 0;
  encoder.counter = 0;
  encoder.direction = 0;
  encoder.button_state = ENC_BTN_READ();
  encoder.button_pressed = 0;

  last_update = HAL_GetTick();
  last_ups_time = HAL_GetTick();

  /* draw static display layout */
  Display_Init();
  /* USER CODE END 2 */

  /* infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    /* --- encoder processing --- */
    uint16_t current_value = ENC_READ();
    int32_t diff = (int32_t)current_value - (int32_t)last_encoder_value;

    /* handle 16-bit counter wraparound */
    if (diff > 32767) diff -= 65536;
    else if (diff < -32768) diff += 65536;

    if (diff != 0) {
        last_encoder_value = current_value;

        /* update direction */
        encoder.direction = (diff > 0) ? 1 : 2;

        /* accumulate raw ticks */
        encoder.counter += diff;

        /* convert ticks into logical steps */
        if (encoder.counter >= ENCODER_MAX_TICKS) {
            encoder.step++;
            encoder.counter -= ENCODER_TICKS_PER_STEP;
            display_dirty = 1;
        }
        else if (encoder.counter <= ENCODER_MIN_TICKS) {
            encoder.step--;
            encoder.counter += ENCODER_TICKS_PER_STEP;
            display_dirty = 1;
        }
        else {
            display_dirty = 1;
        }
    }

    /* --- button processing (polling + debounce) --- */
    static uint8_t last_button = 1;
    static uint32_t button_last_change = 0;
    uint8_t current_button = (uint8_t)ENC_BTN_READ();

    if (current_button != last_button) {
        button_last_change = now;
        last_button = current_button;
    }

    if ((now - button_last_change) > 50) {
        if (current_button != encoder.button_state) {
            encoder.button_state = current_button;
            if (encoder.button_state == 0) {
                encoder.button_pressed = 1;
            }
            display_dirty = 1;
        }
    }

    /* --- button action --- */
    if (encoder.button_pressed) {
        /* reset counters on button press */
        encoder.step = 0;
        encoder.counter = 0;
        ENC_RESET();
        last_encoder_value = 0;
        encoder.button_pressed = 0;
        display_dirty = 1;
    }

    /* --- display update --- */
    if ((now - last_update) >= UPDATE_DELAY_MS || display_dirty) {
        last_update = now;
        LED_TOGGLE();
        Display_Update();
        display_dirty = 0;
    }

    /* --- ups calculation --- */
    ups_counter++;
    if ((now - last_ups_time) >= 1000) {
        ups_value = ups_counter;
        ups_counter = 0;
        last_ups_time = now;
        display_dirty = 1;
    }

    /* --- partial screen update if supported --- */
    if (ssd1306DirtyFlag) {
        ssd1306_UpdateScreenChunk();
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief system clock configuration
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType =
      RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
      RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
  * @brief initialize static display content
  */
void Display_Init(void)
{
    ssd1306_Fill(Black);
    ssd1306_DrawRectangle(0, 0, 127, 63, White);

    ssd1306_SetCursor(10, 0);
    ssd1306_WriteString("EC11 Encoder", Font_7x10, White);

    ssd1306_SetCursor(4, 12);
    ssd1306_WriteString("Step:", Font_6x8, White);

    ssd1306_SetCursor(4, 22);
    ssd1306_WriteString("Counter:", Font_6x8, White);

    ssd1306_SetCursor(4, 32);
    ssd1306_WriteString("Direction:", Font_6x8, White);

    ssd1306_SetCursor(4, 42);
    ssd1306_WriteString("Button:", Font_6x8, White);

    ssd1306_SetCursor(4, 52);
    ssd1306_WriteString("UPS:", Font_6x8, White);

    ssd1306_UpdateScreen();
}

/**
  * @brief update dynamic display content
  */
void Display_Update(void)
{
    /* clear dynamic regions */
    ssd1306_FillRectangle(40, 12, 127, 20, Black);
    ssd1306_FillRectangle(40, 22, 127, 30, Black);
    ssd1306_FillRectangle(40, 32, 127, 40, Black);
    ssd1306_FillRectangle(40, 42, 127, 50, Black);
    ssd1306_FillRectangle(40, 52, 127, 60, Black);

    /* step counter */
    snprintf(display_buf, sizeof(display_buf), "%6ld", encoder.step);
    ssd1306_SetCursor(40, 12);
    ssd1306_WriteString(display_buf, Font_6x8, White);

    /* tick counter */
    snprintf(display_buf, sizeof(display_buf), "%+3ld/%-2d",
             encoder.counter, ENCODER_TICKS_PER_STEP);
    ssd1306_SetCursor(40, 22);
    ssd1306_WriteString(display_buf, Font_6x8, White);

    /* direction */
    const char *dir_text = "---";
    if (encoder.direction == 1) dir_text = "CW ";
    else if (encoder.direction == 2) dir_text = "CCW";

    strncpy(temp_buf, dir_text, sizeof(temp_buf) - 1);
    temp_buf[sizeof(temp_buf) - 1] = '\0';
    ssd1306_SetCursor(40, 32);
    ssd1306_WriteString(temp_buf, Font_6x8, White);

    /* button state */
    const char *btn_text =
        (encoder.button_state == 0) ? "PRESSED" : "RELEASED";
    strncpy(temp_buf, btn_text, sizeof(temp_buf) - 1);
    temp_buf[sizeof(temp_buf) - 1] = '\0';
    ssd1306_SetCursor(40, 42);
    ssd1306_WriteString(temp_buf, Font_6x8, White);

    /* ups */
    snprintf(display_buf, sizeof(display_buf), "%4lu", ups_value);
    ssd1306_SetCursor(40, 52);
    ssd1306_WriteString(display_buf, Font_6x8, White);

    /* gpio diagnostic */
    uint8_t pa0 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
    uint8_t pa1 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);
    snprintf(display_buf, sizeof(display_buf), "A:%d B:%d", pa0, pa1);
    ssd1306_SetCursor(4, 2);
    ssd1306_WriteString(display_buf, Font_6x8, White);

    /* raw tim2 counter value */
    uint16_t raw = ENC_READ();
    snprintf(display_buf, sizeof(display_buf), "TIM2:%u", raw);
    ssd1306_SetCursor(70, 2);
    ssd1306_WriteString(display_buf, Font_6x8, White);

    ssd1306DirtyFlag = 1;
}
/* USER CODE END 4 */

/**
  * @brief error handler
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1) { }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* user can add his own implementation */
}
#endif
