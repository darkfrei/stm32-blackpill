/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Simple OLED test program for SSD1106
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Test_Screen_1(void);
void Test_Screen_2(void);
void Test_Screen_3(void);
void Test_Animation(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_I2C1_Init();

  /* USER CODE BEGIN 2 */

  if (ssd1306_Init() != SSD1306_OK) {
    Error_Handler();
  }

  HAL_Delay(100);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    Test_Screen_1();
    HAL_Delay(5000);

    Test_Screen_2();
    HAL_Delay(5000);

    Test_Screen_3();
    HAL_Delay(5000);

    Test_Animation();
    HAL_Delay(1000);

    /* USER CODE END 3 */
  }
}

/* USER CODE BEGIN 4 */

void Test_Screen_1(void)
{
  ssd1306_Fill(Black);

  ssd1306_DrawRectangle(0, 0, SSD1306_WIDTH - 1, SSD1306_HEIGHT - 1, White);

  ssd1306_SetCursor(10, 10);
  ssd1306_WriteString("SSD1306", Font_11x18, White);

  ssd1306_SetCursor(10, 25);
  ssd1306_WriteString("test screen", Font_7x10, White);

  // Time in seconds
  char buf1[20];
  snprintf(buf1, sizeof(buf1), "Time: %lus", HAL_GetTick() / 1000);
  ssd1306_SetCursor(10, 35);
  ssd1306_WriteString(buf1, Font_6x8, White);

  // Time in milliseconds
  char buf2[20];
  snprintf(buf2, sizeof(buf2), "Time: %lums", HAL_GetTick());
  ssd1306_SetCursor(10, 45);
  ssd1306_WriteString(buf2, Font_6x8, White);

  ssd1306_UpdateScreen();
}
void Test_Screen_2(void)
{
  ssd1306_Fill(Black);

  ssd1306_SetCursor(5, 5);
  ssd1306_WriteString("Test 2: Shapes", Font_7x10, White);

  ssd1306_DrawRectangle(5, 20, 35, 50, White);
  ssd1306_FillRectangle(45, 20, 75, 50, White);

  ssd1306_DrawCircle(100, 35, 15, White);
  ssd1306_FillCircle(100, 35, 7, White);

  ssd1306_UpdateScreen();
}

void Test_Screen_3(void)
{
  ssd1306_Fill(Black);

  ssd1306_DrawRectangle(0, 0, SSD1306_WIDTH - 1, SSD1306_HEIGHT - 1, White);

  ssd1306_SetCursor(5, 5);
  ssd1306_WriteString("Lines Test", Font_7x10, White);

  for (uint8_t i = 0; i <= 5; i++) {
    ssd1306_Line(10, 20 + i * 8, 110, 20 + i * 8, White);
  }



  ssd1306_Line(10, 20, 10, 60, White);
  ssd1306_Line(30, 20, 30, 60, White);
  ssd1306_Line(50, 20, 50, 60, White);
  ssd1306_Line(70, 20, 70, 60, White);
  ssd1306_Line(90, 20, 90, 60, White);
  ssd1306_Line(110, 20, 110, 60, White);

  ssd1306_Line(10, 20, 110, 60, White);
  ssd1306_Line(110, 20, 10, 60, White);

  ssd1306_UpdateScreen();
}

void Test_Animation(void)
{
  uint8_t radius = 8;
  uint8_t x_min = radius + 2;
  uint8_t x_max = SSD1306_WIDTH - radius - 2;
  uint8_t y = 30;

  for (uint8_t cycle = 0; cycle < 2; cycle++) {

    for (uint8_t x = x_min; x < x_max; x += 2) {
      ssd1306_Fill(Black);

      ssd1306_SetCursor(20, 5);
      ssd1306_WriteString("Animation", Font_7x10, White);

      ssd1306_FillCircle(x, y, radius, White);
      ssd1306_DrawRectangle(0, 40, SSD1306_WIDTH - 1, 45, White);

      ssd1306_UpdateScreen();
      HAL_Delay(20);
    }

    for (uint8_t x = x_max; x > x_min; x -= 2) {
      ssd1306_Fill(Black);

      ssd1306_SetCursor(20, 5);
      ssd1306_WriteString("Animation", Font_7x10, White);

      ssd1306_FillCircle(x, y, radius, White);
      ssd1306_DrawRectangle(0, 40, SSD1306_WIDTH - 1, 45, White);

      ssd1306_UpdateScreen();
      HAL_Delay(20);
    }
  }
}

/* USER CODE END 4 */

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
