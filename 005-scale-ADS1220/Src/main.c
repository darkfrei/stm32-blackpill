/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : ADS1220 Digital Scale with Integer Math
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
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "sh1106.h"
#include "sh1106_fonts.h"
#include "EC11.h"
#include "ads1220.h"
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
#define LED_TOGGLE()     HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
ADS1220_Handle_t hads1220;
EC11_Encoder_t encoder;

int32_t adc_code = 0;
int32_t adc_raw = 0;
uint8_t drdy_status = 0;

uint32_t last_update = 0;
uint32_t sample_count = 0;
uint32_t samples_per_sec = 0;
uint32_t last_sps_time = 0;

uint8_t ads_init_ok = 0;
uint8_t reg_check[4] = {0};

char display_buf[64];

/* Scale calibration - integer math */
int32_t tare_offset = 0;
int32_t weight_grams_x10 = 0;
// int32_t calibration_divisor = 180;
int32_t calibration_divisor = 1724;
uint8_t tare_pressed = 0;
uint8_t last_button_state = 1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Display_Init(void);
void Display_Update(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void adsCsLow(void);
static void adsCsHigh(void);
static int  adsSpiTxRx(const uint8_t *tx, uint8_t *rx, uint16_t len);
static uint8_t adsDrdyRead(void);

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
  
  hads1220.csLow   = adsCsLow;
  hads1220.csHigh  = adsCsHigh;
  hads1220.spiTxRx = adsSpiTxRx;
  hads1220.drdyRead= adsDrdyRead;
  hads1220.gain = 128;
  hads1220.vref = 2.048f;

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  /* Small delay for power stabilization */
  HAL_Delay(100);

  /* Initialize OLED display */
  if (SH1106_Init() != SH1106_OK) {
      while (1) {
          LED_TOGGLE();
          HAL_Delay(200);
      }
  }

  SH1106_Fill(SH1106_COLOR_BLACK);
  SH1106_UpdateScreen();

  /* Initialize encoder */
  EC11_Init(&encoder);
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);

  /* Initialize ADS1220 */
  if (ADS1220_Init(&hads1220) == ADS1220_OK) {
      ads_init_ok = 1;
      hads1220.gain = 128;
  } else {
      ads_init_ok = 0;
      SH1106_WriteStringAt(10, 28, "ADS1220 INIT FAIL", Font_8H, SH1106_COLOR_WHITE);
      SH1106_UpdateScreen();
      while(1) {
          LED_TOGGLE();
          HAL_Delay(500);
      }
  }

  /* Read back registers for diagnostics */
  ADS1220_ReadRegister(&hads1220, 0, &reg_check[0]);
  ADS1220_ReadRegister(&hads1220, 1, &reg_check[1]);
  ADS1220_ReadRegister(&hads1220, 2, &reg_check[2]);
  ADS1220_ReadRegister(&hads1220, 3, &reg_check[3]);

  HAL_Delay(500);

  /* Draw initial display layout */
  Display_Init();

  last_update = HAL_GetTick();
  last_sps_time = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    /* Read ADS1220 data */
    if (ads_init_ok) {
        if (ADS1220_ReadData(&hads1220, &adc_raw) == ADS1220_OK) {
            adc_code = adc_raw - tare_offset;
            
            /* Calculate weight using integer math */
            /* weight_grams_x10 = (adc_code * 10) / divisor */
            /* This gives grams with 1 decimal place */
            weight_grams_x10 = (adc_code * 10) / calibration_divisor;
            
            sample_count++;
        }
    }

    /* Check DRDY status */
    drdy_status = ADS1220_DataReady(&hads1220);

    /* Calculate samples per second */
    if ((now - last_sps_time) >= 1000) {
        samples_per_sec = sample_count;
        sample_count = 0;
        last_sps_time = now;
    }

    /* Tare button handling (PA3 - Confirm button) */
    uint8_t button_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3);
    if (button_state == GPIO_PIN_RESET && last_button_state == GPIO_PIN_SET) {
        HAL_Delay(50);
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET) {
            tare_offset = adc_raw;
            tare_pressed = 1;
        }
    }
    last_button_state = button_state;

    /* Update display periodically */
    if ((now - last_update) >= UPDATE_DELAY_MS) {
        last_update = now;
        LED_TOGGLE();
        Display_Update();
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

  /** Initializes the CPU, AHB and APB buses clocks
  */
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

/* USER CODE BEGIN 4 */

/**
  * @brief Initialize static display content
  */
void Display_Init(void)
{
    SH1106_Fill(SH1106_COLOR_BLACK);
    SH1106_DrawRectangle(0, 0, 127, 63, SH1106_COLOR_WHITE);

    SH1106_WriteStringAt(20, 2, "Digital Scale", Font_8H, SH1106_COLOR_WHITE);

    /* Labels */
    SH1106_WriteStringAt(2, 14, "Weight:", Font_8H, SH1106_COLOR_WHITE);
    SH1106_WriteStringAt(2, 26, "ADC:", Font_8H, SH1106_COLOR_WHITE);
    SH1106_WriteStringAt(2, 38, "SPS:", Font_8H, SH1106_COLOR_WHITE);
    SH1106_WriteStringAt(2, 50, "Div:", Font_8H, SH1106_COLOR_WHITE);

    SH1106_UpdateScreen();
}

/**
  * @brief Update dynamic display content
  */
void Display_Update(void)
{
    /* Clear dynamic areas */
    SH1106_FillRectangle(2, 14, 121, 48, SH1106_COLOR_BLACK);

    /* Display weight in grams with 1 decimal */
    if (tare_pressed) {
        int32_t grams = weight_grams_x10 / 10;
        int32_t decimal = weight_grams_x10 % 10;
        if (decimal < 0) decimal = -decimal;
        
        snprintf(display_buf, sizeof(display_buf), "%ld.%ld g", grams, decimal);
    } else {
        snprintf(display_buf, sizeof(display_buf), "TARE 1st");
    }
    SH1106_WriteStringAt(2, 14, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* Display RAW ADC */
    snprintf(display_buf, sizeof(display_buf), "RAW: %ld", adc_raw);
    SH1106_WriteStringAt(2, 24, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* Display ADC code (after tare) */
    snprintf(display_buf, sizeof(display_buf), "ADC: %ld", adc_code);
    SH1106_WriteStringAt(2, 34, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* Display weight x10 for debug */
    snprintf(display_buf, sizeof(display_buf), "x10: %ld", weight_grams_x10);
    SH1106_WriteStringAt(2, 44, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* Display divisor */
    snprintf(display_buf, sizeof(display_buf), "DIV: %ld SPS:%lu", calibration_divisor, samples_per_sec);
    SH1106_WriteStringAt(2, 54, display_buf, Font_8H, SH1106_COLOR_WHITE);

    SH1106_UpdateScreen();
}

static void adsCsLow(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}

static void adsCsHigh(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
}

static int adsSpiTxRx(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    HAL_StatusTypeDef st;
    if (tx && rx) {
        st = HAL_SPI_TransmitReceive(&hspi1, (uint8_t*)tx, rx, len, 100);
    } else if (rx) {
        static uint8_t zeros[256];
        if (len > sizeof(zeros)) return -1;
        st = HAL_SPI_TransmitReceive(&hspi1, zeros, rx, len, 100);
    } else if (tx) {
        st = HAL_SPI_Transmit(&hspi1, (uint8_t*)tx, len, 100);
    } else {
        return -1;
    }
    return (st == HAL_OK) ? 0 : -1;
}

static uint8_t adsDrdyRead(void)
{
    return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET;
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */