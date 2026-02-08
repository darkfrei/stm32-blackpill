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
#include "sh1106.h"           // ← исправлено: строчные буквы
#include "sh1106_fonts.h"     // ← исправлено: строчные буквы
#include "encoder_EC11.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

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
EC11_Encoder_t encoder;

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
  
  /* small delay for power and display startup */
  HAL_Delay(50);

  /* initialize oled */
  if (SH1106_Init() != SH1106_OK) {  // ← исправлено: используем SH1106_OK
      /* blink led forever on oled init failure */
      while (1) {
          LED_TOGGLE();
          HAL_Delay(200);
      }
  }

  SH1106_Fill(SH1106_COLOR_BLACK);
  SH1106_UpdateScreen();

  /* initialize encoder driver */
  EC11_Init(&encoder);

  /* start encoder timer */
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  ENC_RESET();

  last_update = HAL_GetTick();
  last_ups_time = HAL_GetTick();

  /* draw static display layout */
  Display_Init();
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    /* read hardware timer value and get signed diff with wraparound handling */
    int32_t diff = EC11_TimerDiff16(&encoder, ENC_READ());

    /* process encoder rotation through driver */
    if (diff != 0) {
        EC11_ProcessTicks(&encoder, diff);
        display_dirty = 1;
    }

    /* read button and process through driver (use debounce e.g. 50 ms) */
    uint8_t button_raw = (uint8_t)ENC_BTN_READ();
    EC11_ProcessButton(&encoder, button_raw, now, 50);

    /* handle button press event */
    if (encoder.buttonPressed) {
        /* reset encoder and timer */
        EC11_Reset(&encoder);
        ENC_RESET();

        /* reset driver's internal timer baseline */
        encoder.lastTimerValue = 0;

        display_dirty = 1;
    }

    /* update display periodically or when dirty */
    if ((now - last_update) >= UPDATE_DELAY_MS || display_dirty) {
        last_update = now;
        LED_TOGGLE();
        Display_Update();
        display_dirty = 0;
    }

    /* calculate updates per second */
    ups_counter++;
    if ((now - last_ups_time) >= 1000) {
        ups_value = ups_counter;
        ups_counter = 0;
        last_ups_time = now;
        display_dirty = 1;
    }

    /* USER CODE END 3 */
  }
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
  * @brief initialize static display content
  */
void Display_Init(void)
{
    SH1106_Fill(SH1106_COLOR_BLACK);
    SH1106_DrawRectangle(0, 0, 127, 63, SH1106_COLOR_WHITE);

    SH1106_WriteStringAt(20, 2, "EC11 Encoder", Font_8H, SH1106_COLOR_WHITE);
    SH1106_WriteStringAt(4, 22, "Step:", Font_8H, SH1106_COLOR_WHITE);
    SH1106_WriteStringAt(4, 32, "Dir:", Font_8H, SH1106_COLOR_WHITE);
    SH1106_WriteStringAt(4, 42, "Button:", Font_8H, SH1106_COLOR_WHITE);
    SH1106_WriteStringAt(4, 52, "UPS:", Font_8H, SH1106_COLOR_WHITE);

    SH1106_UpdateScreen();
}

/**
  * @brief update dynamic display content
  */
void Display_Update(void)
{
    /* clear dynamic areas to prevent text artifacts */
    SH1106_FillRectangle(4, 12, 120, 8, SH1106_COLOR_BLACK);   // GPIO line
    SH1106_FillRectangle(30, 22, 90, 8, SH1106_COLOR_BLACK);   // Step
    SH1106_FillRectangle(40, 32, 80, 8, SH1106_COLOR_BLACK);   // Direction
    SH1106_FillRectangle(50, 42, 70, 8, SH1106_COLOR_BLACK);   // Button
    SH1106_FillRectangle(40, 52, 80, 8, SH1106_COLOR_BLACK);   // UPS
    
    /* gpio diagnostic (encoder input states) */
    uint8_t pa0 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
    uint8_t pa1 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);
    snprintf(display_buf, sizeof(display_buf), "A:%d B:%d", pa0, pa1);
    SH1106_WriteStringAt(4, 12, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* step counter */
    snprintf(display_buf, sizeof(display_buf), "%ld", encoder.step);
    SH1106_WriteStringAt(45, 22, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* direction */
    const char *dir_text = "---";
    if (encoder.dir == EC11_DIR_CW) {
        dir_text = "CW ";
    } else if (encoder.dir == EC11_DIR_CCW) {
        dir_text = "CCW";
    }
    SH1106_WriteStringAt(40, 32, dir_text, Font_8H, SH1106_COLOR_WHITE);

    /* button state */
    const char *btn_text = (encoder.buttonState == 0) ? "PRESSED " : "RELEASED";
    SH1106_WriteStringAt(50, 42, btn_text, Font_8H, SH1106_COLOR_WHITE);

    /* button press event indicator */
    if (encoder.buttonPressed) {
        SH1106_WriteStringAt(110, 42, "!", Font_8H, SH1106_COLOR_WHITE);
    }

    /* ups */
    snprintf(display_buf, sizeof(display_buf), "%lu", ups_value);
    SH1106_WriteStringAt(40, 52, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* update screen */
    SH1106_UpdateScreen();
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1) { }
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
