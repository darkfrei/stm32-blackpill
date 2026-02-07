
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : LED Blink + OLED status display
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "i2c.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* LED toggle interval (500 ms ON, 500 ms OFF) and redraw buffer every 500 ms */
#define UPDATE_DELAY_MS 500

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint32_t screen_update_counter = 0;
uint32_t last_ups_calc = 0;
uint32_t ups_value = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

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
  /* USER CODE BEGIN 2 */
  MX_I2C1_Init();

  /* initialize oled display */
  if (ssd1306_Init() != SSD1306_OK) {
      /* if oled init fails, blink pc13 rapidly */
      while(1) {
          HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
          HAL_Delay(100);
      }
  }

  /* clear screen */
  ssd1306_Fill(Black);
  ssd1306_UpdateScreen();

  last_ups_calc = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  uint32_t lastUpdate = HAL_GetTick();

  uint8_t led_state = 0;  /* 0 = OFF, 1 = ON */
  char buffer[32];

  /* draw static elements once */
  ssd1306_DrawRectangle(0, 0, SSD1306_WIDTH-1, SSD1306_HEIGHT-1, White);
  ssd1306_SetCursor(4, 10);
  ssd1306_WriteString("Test", Font_7x10, White);
  ssd1306_SetCursor(40, 2);
  ssd1306_WriteString("SSD1306", Font_11x18, White);
  
  /* draw initial dynamic content */
  snprintf(buffer, sizeof(buffer), "LED:%s", led_state ? "ON " : "OFF");
  ssd1306_SetCursor(4, 20);
  ssd1306_WriteString(buffer, Font_6x8, White);
  
  snprintf(buffer, sizeof(buffer), "UPS:%lu", (unsigned long)ups_value);
  ssd1306_SetCursor(4, 30);
  ssd1306_WriteString(buffer, Font_6x8, White);

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    uint32_t now = HAL_GetTick();

    /* toggle led every UPDATE_DELAY_MS and update display content */
    if ((now - lastUpdate) >= UPDATE_DELAY_MS)
    {
      lastUpdate = now;

      // toggle led
      HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

      /* update state (inverted logic: RESET = ON, SET = OFF) */
      led_state = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) ? 1 : 0;

      /* update only changing parts of the display */
      /* clear previous text areas */
      // ssd1306_FillRectangle(4, 20, 4 + 6*8, 20 + 8, Black);
      // ssd1306_FillRectangle(4, 30, 4 + 6*8, 30 + 8, Black);
      
      /* draw updated text */
      snprintf(buffer, sizeof(buffer), "LED:%s", led_state ? "ON " : "OFF");
      ssd1306_SetCursor(4, 20);
      ssd1306_WriteString(buffer, Font_6x8, White);

      snprintf(buffer, sizeof(buffer), "UPS:%lu", (unsigned long)ups_value);
      ssd1306_SetCursor(4, 30);
      ssd1306_WriteString(buffer, Font_6x8, White);
    }

/* send chunk to screen every iteration using dirty updates */
if (ssd1306DirtyFlag) {
    /* update only the changed parts of the screen */
    ssd1306_UpdateDirtyChunk();
} else {
    /* no changes detected - skip update to maximize performance */
    /* note: with UpdateScreenChunk() enabled, UPS is 1000 */
    /* note: with UpdateScreenChunk() commented, UPS is 290000 */
    // ssd1306_UpdateScreenChunk();
}
    screen_update_counter++;

    /* calculate ups every second */
    if ((now - last_ups_calc) >= 1000)
    {
      ups_value = screen_update_counter;
      screen_update_counter = 0;
      last_ups_calc = now;
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

#ifdef SSD1306_USE_DMA
/* i2c dma transfer complete callback */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    /* this function is called automatically by hal when dma transfer completes */
    /* no action needed here, but function must exist */
    (void)hi2c;
}
#endif

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
