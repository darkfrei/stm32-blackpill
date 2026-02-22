/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   006-stroboscope — STM32F411 BlackPill
  *
  * TIM1_CH1 (PA8) — LED brightness PWM, 10 kHz
  * TIM2           — EC11 rotary encoder
  * TIM3           — strobe timer, Update + CC1 interrupts
  * I2C1           — SH1106 display
  *
  * NOTE: TIM3_IRQHandler is defined here.
  * In stm32f4xx_it.c, comment out the body of TIM3_IRQHandler:
  *   void TIM3_IRQHandler(void)
  *   {
  *       // HAL_TIM_IRQHandler(&htim3);   <-- comment this line
  *   }
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "sh1106.h"
#include "sh1106_fonts.h"
#include "EC11.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define UPDATE_DELAY_MS      100

/* Buttons */
#define BTN1_PORT  GPIOA
#define BTN1_PIN   GPIO_PIN_2   /* parameter select */
#define BTN2_PORT  GPIOA
#define BTN2_PIN   GPIO_PIN_3   /* on / off         */
#define BTN3_PORT  GPIOA
#define BTN3_PIN   GPIO_PIN_4   /* reset to default */

/* Strobe parameters */
#define STROBE_FREQ_MIN    10u
#define STROBE_FREQ_MAX   100u
#define STROBE_FREQ_INIT   30u

#define STROBE_DUTY_MIN    1u
#define STROBE_DUTY_MAX   50u
#define STROBE_DUTY_INIT   2u    /* 2% ≈ 1/50 */

#define STROBE_BRIGHT_MIN  10u
#define STROBE_BRIGHT_MAX  100u
#define STROBE_BRIGHT_INIT 75u

/* TIM1: PSC=9, ARR=999 → 10 kHz */
#define TIM1_PWM_ARR       999u
/* TIM3: PSC=9999 → tick 0.1 ms, f_cnt = 10 kHz */
#define TIM3_TICK_FREQ     10000u
/* USER CODE END PD */

/* USER CODE BEGIN PM */
#define LED_TOGGLE()           HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13)
#define BTN_PRESSED(port, pin) (HAL_GPIO_ReadPin((port),(pin)) == GPIO_PIN_RESET)
#define ENC_READ()             ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim2)))
#define ENC_RESET()            (__HAL_TIM_SET_COUNTER(&htim2, 0))
/* USER CODE END PM */

/* USER CODE BEGIN PTD */
typedef enum { ADJ_FREQ = 0, ADJ_DUTY, ADJ_BRIGHT, ADJ_COUNT } AdjMode_t;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       last_state;  /* 1 = not pressed */
    uint8_t       pressed;     /* 1 = just pressed (edge) */
    uint32_t      last_time;
} Button_t;
/* USER CODE END PTD */

/* USER CODE BEGIN PV */
/* Handles from tim.c / i2c.c (CubeMX) */
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern I2C_HandleTypeDef hi2c1;

EC11_Encoder_t encoder;

/* Strobe state */
static uint32_t  g_freq    = STROBE_FREQ_INIT;
static uint32_t  g_duty    = STROBE_DUTY_INIT;
static uint32_t  g_bright  = STROBE_BRIGHT_INIT;
static uint8_t   g_running = 1;
static AdjMode_t g_adj     = ADJ_FREQ;

/* Buttons (polling in main loop) */
static Button_t btn1 = { BTN1_PORT, BTN1_PIN, 1, 0, 0 };
static Button_t btn2 = { BTN2_PORT, BTN2_PIN, 1, 0, 0 };
static Button_t btn3 = { BTN3_PORT, BTN3_PIN, 1, 0, 0 };

static uint32_t last_display_tick = 0;
static char     disp_buf[32];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
static void Strobe_ApplySettings(void);
static void Strobe_SetRunning(uint8_t on);
static void Button_Poll(Button_t *b);
static void Encoder_Process(void);
static void Display_Update(void);
static inline uint32_t Bright_CCR(uint32_t pct);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* -- Calculate CCR1 for TIM1 by brightness percentage ---------------- */
static inline uint32_t Bright_CCR(uint32_t pct)
{
    return ((TIM1_PWM_ARR + 1u) * pct) / 100u;
}

/* -- Apply freq/duty/bright to TIM3 ---------------------------------- */
static void Strobe_ApplySettings(void)
{
    uint32_t arr = (TIM3_TICK_FREQ / g_freq) - 1u;
    uint32_t ccr = ((arr + 1u) * g_duty) / 100u;
    if (ccr == 0u) ccr = 1u;
    if (ccr > arr) ccr = arr;

    HAL_TIM_Base_Stop_IT(&htim3);
    __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_CC1);
    __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ccr);
    __HAL_TIM_SET_COUNTER(&htim3, 0u);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE | TIM_FLAG_CC1);

    if (g_running) {
        HAL_TIM_Base_Start_IT(&htim3);
    }
}

/* -- Turn strobe on / off -------------------------------------------- */
static void Strobe_SetRunning(uint8_t on)
{
    g_running = on;
    if (on) {
        Strobe_ApplySettings();
    } else {
        HAL_TIM_Base_Stop_IT(&htim3);
        __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_CC1);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0u);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    }
}

/* -- Poll button with debounce --------------------------------------- */
static void Button_Poll(Button_t *b)
{
    b->pressed = 0;
    uint32_t now = HAL_GetTick();
    uint8_t raw = BTN_PRESSED(b->port, b->pin) ? 0u : 1u;
    if (raw == 0u && b->last_state == 1u) {
        if ((now - b->last_time) >= BTN_DEBOUNCE_MS) {
            b->pressed   = 1;
            b->last_time = now;
        }
    }
    b->last_state = raw;
}

/* -- Encoder processing --------------------------------------------- */
static void Encoder_Process(void)
{
    int32_t diff = EC11_TimerDiff16(&encoder, ENC_READ());
    if (diff == 0) return;

    int32_t before = encoder.step;
    EC11_ProcessTicks(&encoder, diff);
    int32_t delta = encoder.step - before;
    if (delta == 0) return;

    int32_t step = (delta > 0) ? 1 : -1;

    switch (g_adj) {
    case ADJ_FREQ:
        if (step > 0 && g_freq < STROBE_FREQ_MAX) g_freq++;
        else if (step < 0 && g_freq > STROBE_FREQ_MIN) g_freq--;
        break;
    case ADJ_DUTY:
        if (step > 0 && g_duty < STROBE_DUTY_MAX) g_duty++;
        else if (step < 0 && g_duty > STROBE_DUTY_MIN) g_duty--;
        break;
    case ADJ_BRIGHT:
        if (step > 0 && g_bright < STROBE_BRIGHT_MAX) g_bright++;
        else if (step < 0 && g_bright > STROBE_BRIGHT_MIN) g_bright--;
        break;
    default: break;
    }

    Strobe_ApplySettings();
    last_display_tick = 0;   /* force display update */
}

/* -- Update SH1106 128×64 display ----------------------------------- */
static void Display_Update(void)
{
    SH1106_Fill(SH1106_COLOR_BLACK);

    /* Inverted header */
    SH1106_FillRectangle(0, 0, 127, 11, SH1106_COLOR_WHITE);
    SH1106_WriteStringAt(14, 2, "= STROBE 006 =", Font_8H, SH1106_COLOR_BLACK);

    /* Frequency */
    snprintf(disp_buf, sizeof(disp_buf), "%cFreq: %3lu Hz",
             (g_adj == ADJ_FREQ) ? '>' : ' ', g_freq);
    SH1106_WriteStringAt(0, 14, disp_buf, Font_8H, SH1106_COLOR_WHITE);

    /* Duty */
    uint32_t div_n = (g_duty > 0) ? (100u / g_duty) : 100u;
    snprintf(disp_buf, sizeof(disp_buf), "%cDuty: %2lu%% 1/%lu",
             (g_adj == ADJ_DUTY) ? '>' : ' ', g_duty, div_n);
    SH1106_WriteStringAt(0, 26, disp_buf, Font_8H, SH1106_COLOR_WHITE);

    /* Brightness */
    snprintf(disp_buf, sizeof(disp_buf), "%cBrig: %3lu%%",
             (g_adj == ADJ_BRIGHT) ? '>' : ' ', g_bright);
    SH1106_WriteStringAt(0, 38, disp_buf, Font_8H, SH1106_COLOR_WHITE);

    /* Status bar — inverted */
    SH1106_FillRectangle(0, 51, 127, 63, SH1106_COLOR_WHITE);
    if (g_running)
        SH1106_WriteStringAt(4, 53, "[ ON ]  BTN2=off", Font_8H, SH1106_COLOR_BLACK);
    else
        SH1106_WriteStringAt(4, 53, "[OFF]   BTN2=on ", Font_8H, SH1106_COLOR_BLACK);

    SH1106_UpdateScreen();
}

/* USER CODE END 0 */

/* ════════════════════════════════════════════════════════════════════════
 * TIM3_IRQHandler — strobe timer
 *
 * IMPORTANT: comment out HAL_TIM_IRQHandler(&htim3) in stm32f4xx_it.c !
 * ════════════════════════════════════════════════════════════════════════ */
void TIM3_IRQHandler(void)
{
    /* Update: start of period → LED ON */
    if (__HAL_TIM_GET_FLAG(&htim3, TIM_FLAG_UPDATE) &&
        __HAL_TIM_GET_IT_SOURCE(&htim3, TIM_IT_UPDATE))
    {
        __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, Bright_CCR(g_bright));
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        /* Arm CC1 interrupt for LED off moment */
        __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_CC1);
        __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_CC1);
    }

    /* CC1: duty point reached → LED OFF */
    if (__HAL_TIM_GET_FLAG(&htim3, TIM_FLAG_CC1) &&
        __HAL_TIM_GET_IT_SOURCE(&htim3, TIM_IT_CC1))
    {
        __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_CC1);
        __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_CC1);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0u);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    }
}

/* ════════════════════════════════════════════════════════════════════════
 * main
 * ════════════════════════════════════════════════════════════════════════ */
int main(void)
{
    /* USER CODE BEGIN 1 */
    /* USER CODE END 1 */

    HAL_Init();
    SystemClock_Config();

    /* USER CODE BEGIN Init */
    /* USER CODE END Init */

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_TIM1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();

    /* USER CODE BEGIN 2 */

    /* Display init */
    HAL_Delay(50);
    if (SH1106_Init() != SH1106_OK) {
        while (1) { LED_TOGGLE(); HAL_Delay(200); }
    }
    SH1106_Fill(SH1106_COLOR_BLACK);
    SH1106_WriteStringAt(20, 26, "STROBE 006", Font_8H, SH1106_COLOR_WHITE);
    SH1106_UpdateScreen();
    HAL_Delay(800);

    /* Encoder */
    EC11_Init(&encoder);
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    ENC_RESET();

    /* TIM1 PWM: start, CCR1=0 (LED off) */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0u);

    /* Raise TIM3 priority above buttons */
    HAL_NVIC_SetPriority(TIM3_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);

    /* Start strobe */
    Strobe_ApplySettings();
    Display_Update();

    /* USER CODE END 2 */

    /* ── Main Loop ──────────────────────────────────────────────────── */
    while (1)
    {
        /* USER CODE BEGIN WHILE */

        Button_Poll(&btn1);
        Button_Poll(&btn2);
        Button_Poll(&btn3);

        /* BTN1 — cycle parameter */
        if (btn1.pressed) {
            g_adj = (AdjMode_t)((g_adj + 1u) % ADJ_COUNT);
            last_display_tick = 0;
        }

        /* BTN2 — toggle strobe */
        if (btn2.pressed) {
            Strobe_SetRunning(g_running ? 0u : 1u);
            last_display_tick = 0;
        }

        /* BTN3 — reset defaults */
        if (btn3.pressed) {
            g_freq   = STROBE_FREQ_INIT;
            g_duty   = STROBE_DUTY_INIT;
            g_bright = STROBE_BRIGHT_INIT;
            Strobe_ApplySettings();
            last_display_tick = 0;
        }

        Encoder_Process();

        /* Display refresh */
        uint32_t now = HAL_GetTick();
        if ((now - last_display_tick) >= UPDATE_DELAY_MS) {
            last_display_tick = now;
            Display_Update();
        }

        /* USER CODE END WHILE */
    }
}

/* ════════════════════════════════════════════════════════════════════════
 * SystemClock_Config — HSE 25 MHz → PLL → 100 MHz
 * ════════════════════════════════════════════════════════════════════════ */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 12;
    RCC_OscInitStruct.PLL.PLLN       = 96;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) Error_Handler();
}

/* ════════════════════════════════════════════════════════════════════════
 * Error_Handler
 * ════════════════════════════════════════════════════════════════════════ */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        for (volatile uint32_t i = 0; i < 500000; i++);
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
    Error_Handler();
}
#endif