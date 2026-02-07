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
    int32_t counter;
    uint8_t direction;     /* 0=none, 1=cw, 2=ccw */
    uint8_t button_state;
    uint8_t button_pressed;
} Encoder_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UPDATE_DELAY_MS 200
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
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  /* small delay for peripheral and display power-up */
  HAL_Delay(50);

  /* initialize oled */
  if (ssd1306_Init() != SSD1306_OK) {
      /* blink led to indicate oled init failure */
      while (1) {
          LED_TOGGLE();
          HAL_Delay(200);
      }
  }

  ssd1306_Fill(Black);
  ssd1306_UpdateScreen();

  /* start encoder timer and seed last value */
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  ENC_RESET();

  static uint16_t last_encoder_value = 0;
  last_encoder_value = ENC_READ();

  /* initialize encoder state */
  encoder.counter = 0;
  encoder.direction = 0;
  encoder.button_state = ENC_BTN_READ();
  encoder.button_pressed = 0;

  last_update = HAL_GetTick();
  last_ups_time = HAL_GetTick();

  /* initialize display static content */
  Display_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    /* --- encoder processing --- */
    uint16_t current_value = ENC_READ();
    int32_t diff = (int32_t)current_value - (int32_t)last_encoder_value;

    if (diff > 32767) diff -= 65536;
    else if (diff < -32768) diff += 65536;

    if (diff != 0) {
        encoder.counter += diff;
        encoder.direction = (diff > 0) ? 1 : 2;
        last_encoder_value = current_value;
        display_dirty = 1;
    }

    /* --- button processing (polling with debounce) --- */
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

    /* --- handle button press action --- */
    if (encoder.button_pressed) {
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

    /* --- partial screen flush if driver supports it --- */
    if (ssd1306DirtyFlag) {
        ssd1306_UpdateScreenChunk();
    }

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
  * @brief Initialize display static content
  */
void Display_Init(void)
{
    /* clear screen */
    ssd1306_Fill(Black);

    /* frame and labels */
    ssd1306_DrawRectangle(0, 0, 127, 63, White);

    ssd1306_SetCursor(12, 0);
    ssd1306_WriteString("EC11 Test", Font_7x10, White);

    ssd1306_SetCursor(4, 16);
    ssd1306_WriteString("Counter:", Font_6x8, White);

    ssd1306_SetCursor(4, 26);
    ssd1306_WriteString("Direction:", Font_6x8, White);

    ssd1306_SetCursor(4, 36);
    ssd1306_WriteString("Button:", Font_6x8, White);

    ssd1306_SetCursor(4, 46);
    ssd1306_WriteString("Raw TIM2:", Font_6x8, White);

    ssd1306_SetCursor(4, 56);
    ssd1306_WriteString("UPS:", Font_6x8, White);

    ssd1306_UpdateScreen();
}

/**
  * @brief Update display dynamic content
  */
void Display_Update(void)
{
    /* clear dynamic areas - keep y within 0..63 */
    ssd1306_FillRectangle(60, 16, 127, 24, Black);
    ssd1306_FillRectangle(60, 26, 127, 34, Black);
    ssd1306_FillRectangle(60, 36, 127, 44, Black);
    ssd1306_FillRectangle(60, 46, 127, 54, Black);
    ssd1306_FillRectangle(60, 56, 127, 63, Black);

    /* 1. counter */
    snprintf(display_buf, sizeof(display_buf), "%6ld", encoder.counter);
    ssd1306_SetCursor(60, 16);
    ssd1306_WriteString(display_buf, Font_6x8, White);

    /* 2. direction */
    const char *dir_text = "---";
    if (encoder.direction == 1) dir_text = "CW ";
    else if (encoder.direction == 2) dir_text = "CCW";

    /* copy to mutable buffer to satisfy ssd1306 api */
    strncpy(temp_buf, dir_text, sizeof(temp_buf)-1);
    temp_buf[sizeof(temp_buf)-1] = '\0';
    ssd1306_SetCursor(60, 26);
    ssd1306_WriteString(temp_buf, Font_6x8, White);

    /* 3. button */
    const char *btn_text = (encoder.button_state == 0) ? "PRESSED" : "RELEASED";
    strncpy(temp_buf, btn_text, sizeof(temp_buf)-1);
    temp_buf[sizeof(temp_buf)-1] = '\0';
    ssd1306_SetCursor(60, 36);
    ssd1306_WriteString(temp_buf, Font_6x8, White);

    /* 4. raw tim2 */
    uint16_t raw = ENC_READ();
    snprintf(display_buf, sizeof(display_buf), "%5u", raw);
    ssd1306_SetCursor(60, 46);
    ssd1306_WriteString(display_buf, Font_6x8, White);

    /* 5. ups */
    snprintf(display_buf, sizeof(display_buf), "%4lu", ups_value);
    ssd1306_SetCursor(60, 56);
    ssd1306_WriteString(display_buf, Font_6x8, White);

    /* 6. show gpio states for diagnostic */
    uint8_t pa0 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
    uint8_t pa1 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);
    snprintf(display_buf, sizeof(display_buf), "A:%d B:%d", pa0, pa1);
    ssd1306_SetCursor(4, 6);
    ssd1306_WriteString(display_buf, Font_6x8, White);

    ssd1306DirtyFlag = 1;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
