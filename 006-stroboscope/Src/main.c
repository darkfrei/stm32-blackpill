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
  * Frequency control — two modes:
  *   HZ mode     : 10–100 Hz,  step 1 Hz
  *   PERIOD mode : 150ms–6500ms, step 50ms  (below 10 Hz)
  *     display shows period (ms or s) + calculated Hz
  *     TIM3 tick = 0.1 ms → max ARR = 64999 @ 6500 ms (fits 16-bit)
  *
  * Duty cycle control — two modes:
  *   PCT mode : 5–50%, step 1%
  *   DIV mode : 1/N, N = 25–200, step 5
  *   Display always shows both: "X% 1/N"
  *
  * Apply strategy:
  *   Freq change   → Strobe_ApplyFreq()  — full stop/recalc/restart
  *   Duty change   → Strobe_ApplyDuty()  — CCR only, no counter reset
  *   Bright change → nothing (g_bright read live in TIM3 ISR)
  *
  * NOTE: TIM3_IRQHandler is defined here.
  * In stm32f4xx_it.c comment out the body of TIM3_IRQHandler:
  *   void TIM3_IRQHandler(void)
  *   {
  *       // HAL_TIM_IRQHandler(&htim3);
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
#define BTN2_PIN   GPIO_PIN_3   /* strobe on / off  */
#define BTN3_PORT  GPIOA
#define BTN3_PIN   GPIO_PIN_4   /* reset to default */

/* Frequency — Hz mode */
#define FREQ_HZ_MIN         10u
#define FREQ_HZ_MAX        100u
#define FREQ_HZ_INIT        30u

/* Frequency — period mode (steps of 50 ms, i.e. 0.05 s)
 * step=3  → 150 ms  → 6.67 Hz  (just below 10 Hz boundary)
 * step=130 → 6500 ms → 0.15 Hz  (≈ 6.5 s, ARR = 64999, fits 16-bit) */
#define PERIOD_STEP_MS      50u
#define PERIOD_STEPS_MIN     3u   /* 150 ms */
#define PERIOD_STEPS_MAX   130u   /* 6500 ms */
#define PERIOD_STEPS_INIT    3u

/* Duty — percent mode */
#define DUTY_PCT_MIN         5u
#define DUTY_PCT_MAX        50u
#define DUTY_PCT_INIT        5u

/* Duty — divisor mode: 1/N */
#define DUTY_DIV_MIN        25u
#define DUTY_DIV_MAX       200u
#define DUTY_DIV_STEP        5u

/* Brightness */
#define STROBE_BRIGHT_MIN   10u
#define STROBE_BRIGHT_MAX  100u
#define STROBE_BRIGHT_INIT  75u

/* TIM1: PSC=9, ARR=999 → 10 kHz PWM */
#define TIM1_PWM_ARR       999u

/* TIM3: PSC=9999 → tick = 0.1 ms, f_count = 10 kHz */
#define TIM3_TICK_FREQ   10000u   /* ticks per second */
#define TIM3_TICK_US       100u   /* one tick = 100 µs = 0.1 ms */
/* USER CODE END PD */

/* USER CODE BEGIN PM */
#define LED_TOGGLE()           HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13)
#define BTN_PRESSED(port, pin) (HAL_GPIO_ReadPin((port),(pin)) == GPIO_PIN_RESET)
#define ENC_READ()             ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim2)))
#define ENC_RESET()            (__HAL_TIM_SET_COUNTER(&htim2, 0))
/* USER CODE END PM */

/* USER CODE BEGIN PTD */
typedef enum { ADJ_FREQ = 0, ADJ_DUTY, ADJ_BRIGHT, ADJ_COUNT } AdjMode_t;

/* Frequency mode */
typedef enum { FREQ_MODE_HZ = 0, FREQ_MODE_PERIOD } FreqMode_t;

/* Duty mode */
typedef enum { DUTY_MODE_PCT = 0, DUTY_MODE_DIV } DutyMode_t;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       last_state;
    uint8_t       pressed;
    uint32_t      last_time;
} Button_t;
/* USER CODE END PTD */

/* USER CODE BEGIN PV */
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern I2C_HandleTypeDef hi2c1;

EC11_Encoder_t encoder;

/* Frequency state */
static FreqMode_t g_freq_mode    = FREQ_MODE_HZ;
static uint32_t   g_freq_hz      = FREQ_HZ_INIT;     /* used in HZ mode     */
static uint32_t   g_period_steps = PERIOD_STEPS_INIT; /* used in PERIOD mode */

/* Duty state
 *   DUTY_MODE_PCT : g_duty_val = percent  (5..50)
 *   DUTY_MODE_DIV : g_duty_val = N in 1/N (25..200, step 5) */
static DutyMode_t g_duty_mode = DUTY_MODE_PCT;
static uint32_t   g_duty_val  = DUTY_PCT_INIT;

/* Brightness and run state */
static uint32_t   g_bright  = STROBE_BRIGHT_INIT;
static uint8_t    g_running = 1;
static AdjMode_t  g_adj     = ADJ_FREQ;

/* Buttons */
static Button_t btn1 = { BTN1_PORT, BTN1_PIN, 1, 0, 0 };
static Button_t btn2 = { BTN2_PORT, BTN2_PIN, 1, 0, 0 };
static Button_t btn3 = { BTN3_PORT, BTN3_PIN, 1, 0, 0 };

static uint32_t last_display_tick = 0;
static char     disp_buf[32];
/* USER CODE END PV */

void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
static uint32_t Strobe_GetARR(void);
static void     Strobe_ApplyFreq(void);
static void     Strobe_ApplyDuty(void);
static void     Strobe_SetRunning(uint8_t on);
static uint32_t Duty_GetCCR(uint32_t arr);
static uint32_t Duty_GetPct(void);
static uint32_t Duty_GetDivisor(void);
static void     Duty_Increase(void);
static void     Duty_Decrease(void);
static void     Freq_Increase(void);
static void     Freq_Decrease(void);
static void     Button_Poll(Button_t *b);
static void     Encoder_Process(void);
static void     Display_Update(void);
static inline uint32_t Bright_CCR(uint32_t pct);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* ── Brightness: percent → TIM1 CCR1 ────────────────────────────────────── */
static inline uint32_t Bright_CCR(uint32_t pct)
{
    return ((TIM1_PWM_ARR + 1u) * pct) / 100u;
}

/* ── TIM3 ARR from current frequency/period state ────────────────────────── */
static uint32_t Strobe_GetARR(void)
{
    if (g_freq_mode == FREQ_MODE_HZ) {
        /* Hz mode: ARR = ticks_per_second / freq - 1 */
        return (TIM3_TICK_FREQ / g_freq_hz) - 1u;
    } else {
        /* Period mode: period_ms * 10 ticks/ms - 1
         * tick = 0.1 ms → 10 ticks per ms */
        return (g_period_steps * PERIOD_STEP_MS * 10u) - 1u;
    }
}

/* ── Duty helpers ─────────────────────────────────────────────────────────── */

/* Always returns duty as integer percent (for display) */
static uint32_t Duty_GetPct(void)
{
    if (g_duty_mode == DUTY_MODE_PCT)
        return g_duty_val;
    /* DIV mode: round(100 / N) */
    return (100u + g_duty_val / 2u) / g_duty_val;
}

/* Always returns 1/N divisor (for display) */
static uint32_t Duty_GetDivisor(void)
{
    if (g_duty_mode == DUTY_MODE_DIV)
        return g_duty_val;
    /* PCT mode: round(100 / pct) */
    return (100u + g_duty_val / 2u) / g_duty_val;
}

/* TIM3 CCR1 from current duty state */
static uint32_t Duty_GetCCR(uint32_t arr)
{
    uint32_t ccr;
    if (g_duty_mode == DUTY_MODE_PCT)
        ccr = ((arr + 1u) * g_duty_val) / 100u;
    else
        ccr = (arr + 1u) / g_duty_val;
    if (ccr == 0u) ccr = 1u;
    if (ccr > arr) ccr = arr;
    return ccr;
}

/* ── Duty navigation ──────────────────────────────────────────────────────── */

static void Duty_Increase(void)
{
    if (g_duty_mode == DUTY_MODE_DIV) {
        if (g_duty_val > DUTY_DIV_MIN)
            g_duty_val -= DUTY_DIV_STEP;
        else {
            g_duty_mode = DUTY_MODE_PCT;
            g_duty_val  = DUTY_PCT_MIN;   /* cross to 5% */
        }
    } else {
        if (g_duty_val < DUTY_PCT_MAX)
            g_duty_val++;
    }
}

static void Duty_Decrease(void)
{
    if (g_duty_mode == DUTY_MODE_PCT) {
        if (g_duty_val > DUTY_PCT_MIN)
            g_duty_val--;
        else {
            g_duty_mode = DUTY_MODE_DIV;
            g_duty_val  = DUTY_DIV_MIN;   /* cross to 1/25 */
        }
    } else {
        if (g_duty_val < DUTY_DIV_MAX)
            g_duty_val += DUTY_DIV_STEP;
    }
}

/* ── Frequency navigation ─────────────────────────────────────────────────── */

static void Freq_Increase(void)
{
    if (g_freq_mode == FREQ_MODE_PERIOD) {
        if (g_period_steps > PERIOD_STEPS_MIN)
            g_period_steps--;
        else {
            /* cross boundary: switch to Hz mode at 10 Hz */
            g_freq_mode = FREQ_MODE_HZ;
            g_freq_hz   = FREQ_HZ_MIN;
        }
    } else {
        if (g_freq_hz < FREQ_HZ_MAX)
            g_freq_hz++;
    }
}

static void Freq_Decrease(void)
{
    if (g_freq_mode == FREQ_MODE_HZ) {
        if (g_freq_hz > FREQ_HZ_MIN)
            g_freq_hz--;
        else {
            /* cross boundary: switch to period mode at 150 ms */
            g_freq_mode    = FREQ_MODE_PERIOD;
            g_period_steps = PERIOD_STEPS_MIN;
        }
    } else {
        if (g_period_steps < PERIOD_STEPS_MAX)
            g_period_steps++;
    }
}

/* ── Apply frequency change: full TIM3 stop / reconfigure / restart ──────── */
static void Strobe_ApplyFreq(void)
{
    uint32_t arr = Strobe_GetARR();
    uint32_t ccr = Duty_GetCCR(arr);

    HAL_TIM_Base_Stop_IT(&htim3);
    __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_CC1);
    __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ccr);
    __HAL_TIM_SET_COUNTER(&htim3, 0u);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE | TIM_FLAG_CC1);

    if (g_running)
        HAL_TIM_Base_Start_IT(&htim3);
}

/* ── Apply duty change: update CCR only, no counter reset ───────────────── */
static void Strobe_ApplyDuty(void)
{
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim3);
    uint32_t ccr = Duty_GetCCR(arr);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ccr);
}

/* ── Turn strobe on / off ─────────────────────────────────────────────────── */
static void Strobe_SetRunning(uint8_t on)
{
    g_running = on;
    if (on) {
        Strobe_ApplyFreq();
    } else {
        HAL_TIM_Base_Stop_IT(&htim3);
        __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_CC1);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0u);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    }
}

/* ── Poll button with debounce ────────────────────────────────────────────── */
static void Button_Poll(Button_t *b)
{
    b->pressed = 0;
    uint32_t now = HAL_GetTick();
    uint8_t  raw = BTN_PRESSED(b->port, b->pin) ? 0u : 1u;
    if (raw == 0u && b->last_state == 1u) {
        if ((now - b->last_time) >= BTN_DEBOUNCE_MS) {
            b->pressed   = 1;
            b->last_time = now;
        }
    }
    b->last_state = raw;
}

/* ── Encoder processing ───────────────────────────────────────────────────── */
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
        if (step > 0) Freq_Increase();
        else          Freq_Decrease();
        Strobe_ApplyFreq();   /* ARR changes — full restart */
        break;

    case ADJ_DUTY:
        if (step > 0) Duty_Increase();
        else          Duty_Decrease();
        Strobe_ApplyDuty();   /* CCR only — no counter reset */
        break;

    case ADJ_BRIGHT:
        if (step > 0 && g_bright < STROBE_BRIGHT_MAX) g_bright++;
        else if (step < 0 && g_bright > STROBE_BRIGHT_MIN) g_bright--;
        /* g_bright is read live in TIM3 ISR — nothing to apply */
        break;

    default: break;
    }
}

/* ── Update SH1106 128×64 display ────────────────────────────────────────── */
static void Display_Update(void)
{
    SH1106_Fill(SH1106_COLOR_BLACK);

    /* Inverted header */
    SH1106_FillRectangle(0, 0, 127, 11, SH1106_COLOR_WHITE);
    SH1106_WriteStringAt(14, 2, "= STROBE 006 =", Font_8H, SH1106_COLOR_BLACK);

    /* Frequency / period row */
    if (g_freq_mode == FREQ_MODE_HZ) {
        snprintf(disp_buf, sizeof(disp_buf), "%cFreq: %3lu Hz",
                 (g_adj == ADJ_FREQ) ? '>' : ' ', g_freq_hz);
    } else {
        uint32_t period_ms = g_period_steps * PERIOD_STEP_MS;
        /* freq * 100 (integer, avoids floats) */
        uint32_t fq100 = 100000u / period_ms;

        char freq_str[10];
        if (fq100 >= 100u) {
            /* >= 1.00 Hz: show one decimal, e.g. "6.6Hz" */
            snprintf(freq_str, sizeof(freq_str), "%lu.%luHz",
                     fq100 / 100u, (fq100 % 100u) / 10u);
        } else {
            /* < 1.00 Hz: show two decimals, e.g. "0.15Hz" */
            snprintf(freq_str, sizeof(freq_str), "0.%02luHz", fq100);
        }

        if (period_ms < 1000u) {
            snprintf(disp_buf, sizeof(disp_buf), "%c%lums %s",
                     (g_adj == ADJ_FREQ) ? '>' : ' ', period_ms, freq_str);
        } else {
            uint32_t s_int  = period_ms / 1000u;
            uint32_t s_frac = (period_ms % 1000u) / 10u;  /* two decimal places */
            snprintf(disp_buf, sizeof(disp_buf), "%c%lu.%02lus %s",
                     (g_adj == ADJ_FREQ) ? '>' : ' ', s_int, s_frac, freq_str);
        }
    }
    SH1106_WriteStringAt(0, 14, disp_buf, Font_8H, SH1106_COLOR_WHITE);

    /* Duty — always show both percent and 1/N */
    uint32_t pct = Duty_GetPct();
    uint32_t div = Duty_GetDivisor();
    snprintf(disp_buf, sizeof(disp_buf), "%cDuty:%2lu%% 1/%lu",
             (g_adj == ADJ_DUTY) ? '>' : ' ', pct, div);
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

/* ════════════════════════════════════════════════════════════════════════════
 * TIM3_IRQHandler — strobe timer
 *
 * IMPORTANT: comment out HAL_TIM_IRQHandler(&htim3) in stm32f4xx_it.c !
 * ════════════════════════════════════════════════════════════════════════════ */
void TIM3_IRQHandler(void)
{
    /* Update: start of period → LED ON */
    if (__HAL_TIM_GET_FLAG(&htim3, TIM_FLAG_UPDATE) &&
        __HAL_TIM_GET_IT_SOURCE(&htim3, TIM_IT_UPDATE))
    {
        __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
        /* g_bright is read live — brightness change needs no extra apply step */
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, Bright_CCR(g_bright));
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        /* Arm CC1 interrupt for the LED-off moment */
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

/* ════════════════════════════════════════════════════════════════════════════
 * main
 * ════════════════════════════════════════════════════════════════════════════ */
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

    /* TIM1 PWM: start with CCR1=0 (LED off until first strobe pulse) */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0u);

    /* Raise TIM3 priority above button EXTIs */
    HAL_NVIC_SetPriority(TIM3_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);

    /* Apply initial settings and show display */
    Strobe_ApplyFreq();
    Display_Update();

    /* USER CODE END 2 */

    /* ── Main loop ──────────────────────────────────────────────────────── */
    while (1)
    {
        /* USER CODE BEGIN WHILE */

        Button_Poll(&btn1);
        Button_Poll(&btn2);
        Button_Poll(&btn3);

        /* BTN1 — cycle editable parameter */
        if (btn1.pressed)
            g_adj = (AdjMode_t)((g_adj + 1u) % ADJ_COUNT);

        /* BTN2 — toggle strobe on / off */
        if (btn2.pressed)
            Strobe_SetRunning(g_running ? 0u : 1u);

        /* BTN3 — reset all parameters to defaults */
        if (btn3.pressed) {
            g_freq_mode    = FREQ_MODE_HZ;
            g_freq_hz      = FREQ_HZ_INIT;
            g_period_steps = PERIOD_STEPS_INIT;
            g_duty_mode    = DUTY_MODE_PCT;
            g_duty_val     = DUTY_PCT_INIT;
            g_bright       = STROBE_BRIGHT_INIT;
            Strobe_ApplyFreq();
        }

        Encoder_Process();

        /* Periodic display refresh — fixed rate */
        uint32_t now = HAL_GetTick();
        if ((now - last_display_tick) >= UPDATE_DELAY_MS) {
            last_display_tick = now;
            Display_Update();
        }

        /* USER CODE END WHILE */
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 * SystemClock_Config — HSE 25 MHz → PLL → SYSCLK 100 MHz
 * ════════════════════════════════════════════════════════════════════════════ */
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

/* ════════════════════════════════════════════════════════════════════════════
 * Error_Handler
 * ════════════════════════════════════════════════════════════════════════════ */
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
