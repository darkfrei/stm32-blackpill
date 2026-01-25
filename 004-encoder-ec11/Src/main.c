/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "ssd1306.h"
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
volatile int32_t encoder_value = 0;
volatile uint8_t encoder_button_pressed = 0;
volatile uint16_t encoder_state = 0;

/* led blink state variables (pc13, active low) */
volatile uint8_t led_blink_request = 0;
volatile uint8_t led_blink_active = 0;
volatile uint32_t led_blink_start = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*
 * exti callback: only set flags and timestamps
 * no blocking calls here
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint8_t request = 0;

    /* encoder rotation: pa0 / pa1 */
    if (GPIO_Pin == GPIO_PIN_0 || GPIO_Pin == GPIO_PIN_1)
    {
        uint8_t clk = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
        uint8_t dt  = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);

        uint8_t current = (clk << 1) | dt;
        encoder_state = (encoder_state << 2) | current;

        uint8_t pattern = encoder_state & 0xFF;

        if (pattern == 0xE4)
        {
            encoder_value++;
            encoder_state = 0;
            request = 1;
        }
        else if (pattern == 0xD2)
        {
            encoder_value--;
            encoder_state = 0;
            request = 1;
        }
    }

    /* encoder button: pa2 */
    if (GPIO_Pin == GPIO_PIN_2)
    {
        static uint32_t last_press = 0;
        uint32_t now = HAL_GetTick();

        if ((now - last_press) > 200)
        {
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_RESET)
            {
                encoder_button_pressed = 1;
                encoder_value = 0;
                last_press = now;
                request = 1;
            }
        }
    }

    if (request)
    {
        led_blink_request = 1;
    }
}
/* USER CODE END 0 */

/**
  * @brief  the application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  char buffer[32];
  /* USER CODE END 1 */

  /* mcu configuration--------------------------------------------------------*/

  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_I2C1_Init();

  /* USER CODE BEGIN 2 */

  /* ensure led is off (pc13 active low) */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  if (SSD1306_Init() == 0)
  {
      SSD1306_Clear();
      SSD1306_GotoXY(0, 0);
      SSD1306_Puts("Encoder EN11", 1);
      SSD1306_GotoXY(0, 20);
      SSD1306_Puts("Value:", 1);
      SSD1306_UpdateScreen();
  }
  else
  {
      while (1);
  }

  /* USER CODE END 2 */

  /* infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    snprintf(buffer, sizeof(buffer), "Value: %ld    ", encoder_value);
    SSD1306_GotoXY(0, 20);
    SSD1306_Puts(buffer, 1);

    if (encoder_button_pressed)
    {
        SSD1306_GotoXY(0, 40);
        SSD1306_Puts("PRESSED!", 1);
        encoder_button_pressed = 0;
    }
    else
    {
        SSD1306_GotoXY(0, 40);
        SSD1306_Puts("        ", 1);
    }

    /* non-blocking led blink */
    if (led_blink_request && !led_blink_active)
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        led_blink_active = 1;
        led_blink_request = 0;
        led_blink_start = HAL_GetTick();
    }

    if (led_blink_active)
    {
        if ((HAL_GetTick() - led_blink_start) >= 100)
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
            led_blink_active = 0;
        }
    }

    SSD1306_UpdateScreen();
    HAL_Delay(50);
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief system clock configuration
  * @retval none
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                               RCC_CLOCKTYPE_SYSCLK |
                               RCC_CLOCKTYPE_PCLK1 |
                               RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
