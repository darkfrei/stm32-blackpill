/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   006-stroboscope -- STM32F411 BlackPill
  *
  * TIM1_CH1 (PA8) -- LED brightness PWM, 10 kHz
  * TIM2           -- EC11 rotary encoder (PA0/PA1)
  * TIM3           -- strobe timer, Update + CC1 interrupts
  * I2C1           -- SH1106 display (PB6/PB7)
  *
  * Display note: pixel rows 1-8 (1-indexed from top) are partially broken.
  * Nothing is drawn above y=11 (0-indexed). Four pixels of usable space at
  * the bottom (y=60..63) are used by the status bar.
  *
  * Frequency is stored as millihertz (g_freq_mhz). The encoder step
  * multiplier controls the Hz-linear step per detent:
  *   x1   ->  0.1 Hz / click
  *   x10  ->  1   Hz / click
  *   x100 -> 10   Hz / click
  *
  *   Range: 153 mHz (approx 0.15 Hz) to 1000 Hz.
  *
  * Duty cycle -- two modes:
  *   PERC mode : 5-50%, step 1%
  *   DIV mode  : 1/N, N=25-200, step 5
  *   Display always shows both: "X% 1/N"
  *
  * Brightness -- two modes:
  *   PERC mode : 10-100%, step 1%
  *   DIV mode  : 1/N, N=10-200, step 1
  *   Display always shows both: "X% 1/N"
  *
  * Controls:
  *   Encoder push  -- short: cycle step multiplier (x1 / x10 / x100)
  *                    hold:  save to flash
  *   Top button    -- short: cycle active parameter (Freq->Duty->Bright)
  *   Bottom button -- short: strobe on / off
  *                    hold:  reset all to defaults
  *
  * Flash storage (sector 7, 0x08060000, 128 KB).
  *
  * NOTE: TIM3_IRQHandler is defined here (USER CODE 0).
  * In stm32f4xx_it.c comment out the body of TIM3_IRQHandler:
  *   void TIM3_IRQHandler(void)
  *   {
  *       // HAL_TIM_IRQHandler(&htim3);
  *   }
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
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

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum { ADJ_FREQ = 0, ADJ_DUTY, ADJ_BRIGHT, ADJ_COUNT } AdjMode_t;
typedef enum { DUTY_MODE_PERC = 0, DUTY_MODE_DIV }             DutyMode_t;
typedef enum { BRIG_MODE_PERC = 0, BRIG_MODE_DIV }             BrigMode_t;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       last_state;   /* debounced level: 0=pressed, 1=released */
    uint8_t       pressed;      /* one-shot: short press completed        */
    uint8_t       held;         /* one-shot: hold threshold crossed       */
    uint8_t       long_fired;   /* prevents held firing more than once    */
    uint32_t      last_time;    /* debounce timestamp                     */
    uint32_t      press_time;   /* timestamp of current press start       */
} Button_t;

/* settings saved to flash */
typedef struct {
    uint32_t  magic;          /* identifies a valid, current-layout record */
    uint32_t  freq_mhz;       /* frequency in millihertz                   */
    uint8_t   step_idx;       /* index into g_step_mults[]                 */
    uint8_t   duty_mode;      /* DutyMode_t                                */
    uint8_t   _pad0[2];
    uint32_t  duty_val;
    uint8_t   brig_mode;      /* BrigMode_t                                */
    uint8_t   _pad1[3];
    uint32_t  brig_val;
    uint8_t   running;
    uint8_t   _pad2[3];
    uint32_t  checksum;       /* simple XOR of all preceding bytes         */
} FlashConfig_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UPDATE_DELAY_MS      100u

/* buttons.
 * BTN1 = encoder push (PA2): short=cycle step, hold=save.
 * BTN2 = top    (PA3): short=cycle parameter.
 * BTN3 = bottom (PA4): short=on/off, hold=reset. */
#define BTN1_PORT  GPIOA
#define BTN1_PIN   GPIO_PIN_2
#define BTN2_PORT  GPIOA
#define BTN2_PIN   GPIO_PIN_3
#define BTN3_PORT  GPIOA
#define BTN3_PIN   GPIO_PIN_4

/* frequency in millihertz.
 * At FREQ_MHZ_MIN=153: period_ticks = 10000000/153 = 65359, ARR=65358 (16-bit ok).
 * At FREQ_MHZ_MAX=1000000: period_ticks = 10, ARR = 9. */
#define FREQ_MHZ_MIN         153u
#define FREQ_MHZ_MAX     1000000u
#define FREQ_MHZ_INIT      30000u   /* 30.0 Hz */

/* encoder step multiplier base for frequency: x1 = 100 mHz = 0.1 Hz/click */
#define FREQ_STEP_BASE_MHZ   100u

/* duty -- percent mode */
#define DUTY_PERC_MIN          5u
#define DUTY_PERC_MAX         50u
#define DUTY_PERC_INIT         5u

/* duty -- divisor mode 1/N */
#define DUTY_DIV_MIN          25u
#define DUTY_DIV_MAX         200u
#define DUTY_DIV_STEP          5u

/* brightness -- percent mode */
#define BRIG_PERC_MIN         10u
#define BRIG_PERC_MAX        100u
#define BRIG_PERC_INIT        75u

/* brightness -- divisor mode 1/N */
#define BRIG_DIV_MIN          10u
#define BRIG_DIV_MAX         200u
#define BRIG_DIV_STEP          1u

/* TIM1: PSC=9, ARR=999 -> 10 kHz PWM */
#define TIM1_PWM_ARR         999u

/* TIM3: PSC=9999 -> tick = 0.1 ms, f_count = 10 kHz.
 * period_ticks = 10,000,000 mHz / freq_mhz */
#define TIM3_MHZ_RATE     10000000u

/* flash -- last sector of STM32F411CE (512 KB). */
#define FLASH_CONFIG_SECTOR  FLASH_SECTOR_7
#define FLASH_CONFIG_ADDR    0x08060000UL
#define FLASH_CONFIG_MAGIC   0x5752B008UL

/* notification duration */
#define NOTIFY_DURATION_MS   600u

/* display layout -- y=0..7 (rows 1-8, 1-indexed) are partially broken.
 * All content starts at y=11 or below. Four spare pixels at the bottom
 * (y=60..63) are used by the status bar so rows fit with comfortable gaps. */
#define ROW_STEP_Y            11
#define ROW_FREQ_Y            21
#define ROW_DUTY_Y            31
#define ROW_BRIGHT_Y          41
#define STATUSBAR_Y0          53
#define STATUSBAR_Y1          63
#define STATUSBAR_TEXT_Y      55
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define LED_TOGGLE()           HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13)
#define BTN_PRESSED(port, pin) (HAL_GPIO_ReadPin((port),(pin)) == GPIO_PIN_RESET)
#define ENC_READ()             ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim2)))
#define ENC_RESET()            (__HAL_TIM_SET_COUNTER(&htim2, 0))
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern I2C_HandleTypeDef hi2c1;

EC11_Encoder_t encoder;

/* encoder step multiplier -- x1 / x10 / x100 */
static const uint32_t g_step_mults[3] = { 1u, 10u, 100u };
static uint8_t        g_step_idx      = 0u;

/* frequency state */
static uint32_t   g_freq_mhz  = FREQ_MHZ_INIT;

/* duty state */
static DutyMode_t g_duty_mode = DUTY_MODE_PERC;
static uint32_t   g_duty_val  = DUTY_PERC_INIT;

/* brightness state */
static BrigMode_t g_brig_mode = BRIG_MODE_PERC;
static uint32_t   g_brig_val  = BRIG_PERC_INIT;

static uint8_t    g_running = 1;
static AdjMode_t  g_adj     = ADJ_FREQ;

/* buttons */
static Button_t btn1 = { BTN1_PORT, BTN1_PIN, 1, 0, 0, 0, 0, 0 };
static Button_t btn2 = { BTN2_PORT, BTN2_PIN, 1, 0, 0, 0, 0, 0 };
static Button_t btn3 = { BTN3_PORT, BTN3_PIN, 1, 0, 0, 0, 0, 0 };

/* notification -- init to far past so it never fires at startup */
static char     notify_msg[17]   = "";
static uint32_t notify_time      = (uint32_t)(-NOTIFY_DURATION_MS - 1u);

static uint32_t last_display_tick = 0;
static char     disp_buf[32];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
static uint32_t  Strobe_GetARR(void);
static void      Strobe_ApplyFreq(void);
static void      Strobe_ApplyDuty(void);
static void      Strobe_SetRunning(uint8_t on);
static uint32_t  Duty_GetCCR(uint32_t arr);
static uint32_t  Duty_GetPerc(void);
static uint32_t  Duty_GetDivisor(void);
static void      Duty_Increase(void);
static void      Duty_Decrease(void);
static uint32_t  Brig_GetCCR(void);
static uint32_t  Brig_GetPerc(void);
static uint32_t  Brig_GetDivisor(void);
static void      Brig_Increase(void);
static void      Brig_Decrease(void);
static void      Format_Freq(char *buf, size_t bufsz);
static void      Apply_Defaults(void);
static void      Button_Poll(Button_t *b);
static void      Encoder_Process(void);
static void      Display_Update(void);
static uint32_t  Config_Checksum(const FlashConfig_t *c);
static void      Flash_SaveConfig(void);
static void      Flash_LoadConfig(void);
static void      Notify(const char *msg);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* brightness helpers */
static uint32_t Brig_GetPerc(void)
{
    if (g_brig_mode == BRIG_MODE_PERC) return g_brig_val;
    return (100u + g_brig_val / 2u) / g_brig_val;
}

static uint32_t Brig_GetDivisor(void)
{
    if (g_brig_mode == BRIG_MODE_DIV) return g_brig_val;
    return (100u + g_brig_val / 2u) / g_brig_val;
}

static uint32_t Brig_GetCCR(void)
{
    uint32_t ccr;
    if (g_brig_mode == BRIG_MODE_PERC) {
        ccr = ((TIM1_PWM_ARR + 1u) * g_brig_val) / 100u;
    } else {
        ccr = (TIM1_PWM_ARR + 1u) / g_brig_val;
    }
    if (ccr == 0u) ccr = 1u;
    return ccr;
}

static void Brig_Increase(void)
{
    if (g_brig_mode == BRIG_MODE_DIV) {
        if (g_brig_val > BRIG_DIV_MIN) g_brig_val -= BRIG_DIV_STEP;
        else { g_brig_mode = BRIG_MODE_PERC; g_brig_val = BRIG_PERC_MIN; }
    } else {
        if (g_brig_val < BRIG_PERC_MAX) g_brig_val++;
    }
}

static void Brig_Decrease(void)
{
    if (g_brig_mode == BRIG_MODE_PERC) {
        if (g_brig_val > BRIG_PERC_MIN) g_brig_val--;
        else { g_brig_mode = BRIG_MODE_DIV; g_brig_val = BRIG_DIV_MIN; }
    } else {
        if (g_brig_val < BRIG_DIV_MAX) g_brig_val += BRIG_DIV_STEP;
    }
}

/* duty helpers */
static uint32_t Duty_GetPerc(void)
{
    if (g_duty_mode == DUTY_MODE_PERC) return g_duty_val;
    return (100u + g_duty_val / 2u) / g_duty_val;
}

static uint32_t Duty_GetDivisor(void)
{
    if (g_duty_mode == DUTY_MODE_DIV) return g_duty_val;
    return (100u + g_duty_val / 2u) / g_duty_val;
}

static uint32_t Duty_GetCCR(uint32_t arr)
{
    uint32_t ccr = (g_duty_mode == DUTY_MODE_PERC)
                   ? ((arr + 1u) * g_duty_val) / 100u
                   : (arr + 1u) / g_duty_val;
    if (ccr == 0u) ccr = 1u;
    if (ccr > arr) ccr = arr;
    return ccr;
}

static void Duty_Increase(void)
{
    if (g_duty_mode == DUTY_MODE_DIV) {
        if (g_duty_val > DUTY_DIV_MIN) g_duty_val -= DUTY_DIV_STEP;
        else { g_duty_mode = DUTY_MODE_PERC; g_duty_val = DUTY_PERC_MIN; }
    } else {
        if (g_duty_val < DUTY_PERC_MAX) g_duty_val++;
    }
}

static void Duty_Decrease(void)
{
    if (g_duty_mode == DUTY_MODE_PERC) {
        if (g_duty_val > DUTY_PERC_MIN) g_duty_val--;
        else { g_duty_mode = DUTY_MODE_DIV; g_duty_val = DUTY_DIV_MIN; }
    } else {
        if (g_duty_val < DUTY_DIV_MAX) g_duty_val += DUTY_DIV_STEP;
    }
}

/* frequency helpers */
static uint32_t Strobe_GetARR(void)
{
    uint32_t ticks = (TIM3_MHZ_RATE + g_freq_mhz / 2u) / g_freq_mhz;
    return ticks - 1u;
}

static void Format_Freq(char *buf, size_t bufsz)
{
    uint32_t hz_int  = g_freq_mhz / 1000u;
    uint32_t hz_frac = g_freq_mhz % 1000u;
    uint32_t d1, d2;

    if (hz_int >= 100u) {
        snprintf(buf, bufsz, "%luHz", (g_freq_mhz + 500u) / 1000u);
    } else if (hz_int >= 10u) {
        d1 = (hz_frac + 50u) / 100u;
        if (d1 >= 10u) { hz_int++; d1 = 0u; }
        snprintf(buf, bufsz, "%lu.%luHz", hz_int, d1);
    } else if (hz_int >= 1u) {
        d2 = (hz_frac + 5u) / 10u;
        if (d2 >= 100u) { hz_int++; d2 = 0u; }
        snprintf(buf, bufsz, "%lu.%02luHz", hz_int, d2);
    } else {
        d2 = (hz_frac + 5u) / 10u;
        if (d2 >= 100u) {
            snprintf(buf, bufsz, "1.00Hz");
        } else {
            snprintf(buf, bufsz, "0.%02luHz", d2);
        }
    }
}

/* strobe apply */
static void Strobe_ApplyFreq(void)
{
    uint32_t arr = Strobe_GetARR();
    uint32_t ccr = Duty_GetCCR(arr);

    HAL_TIM_Base_Stop_IT(&htim3);
    __HAL_TIM_DISABLE_IT(&htim3, TIM_IT_CC1);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0u);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

    __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ccr);
    __HAL_TIM_SET_COUNTER(&htim3, 0u);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE | TIM_FLAG_CC1);
    if (g_running) HAL_TIM_Base_Start_IT(&htim3);
}

static void Strobe_ApplyDuty(void)
{
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim3);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, Duty_GetCCR(arr));
}

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

/* defaults */
static void Apply_Defaults(void)
{
    g_freq_mhz  = FREQ_MHZ_INIT;
    g_step_idx  = 0u;
    g_duty_mode = DUTY_MODE_PERC;
    g_duty_val  = DUTY_PERC_INIT;
    g_brig_mode = BRIG_MODE_PERC;
    g_brig_val  = BRIG_PERC_INIT;
    Strobe_ApplyFreq();
}

/* flash config */
static uint32_t Config_Checksum(const FlashConfig_t *c)
{
    const uint8_t *p   = (const uint8_t *)c;
    uint32_t       len = sizeof(FlashConfig_t) - sizeof(uint32_t);
    uint32_t       crc = 0u;
    for (uint32_t i = 0u; i < len; i++) crc ^= (uint32_t)p[i];
    return crc;
}

static void Flash_SaveConfig(void)
{
    FlashConfig_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic     = FLASH_CONFIG_MAGIC;
    cfg.freq_mhz  = g_freq_mhz;
    cfg.step_idx  = g_step_idx;
    cfg.duty_mode = (uint8_t)g_duty_mode;
    cfg.duty_val  = g_duty_val;
    cfg.brig_mode = (uint8_t)g_brig_mode;
    cfg.brig_val  = g_brig_val;
    cfg.running   = g_running;
    cfg.checksum  = Config_Checksum(&cfg);

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Sector       = FLASH_CONFIG_SECTOR,
        .NbSectors    = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3
    };
    uint32_t sector_error = 0u;
    HAL_FLASHEx_Erase(&erase, &sector_error);

    const uint32_t *src  = (const uint32_t *)&cfg;
    uint32_t        addr = FLASH_CONFIG_ADDR;
    for (uint32_t i = 0u; i < sizeof(FlashConfig_t) / 4u; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, src[i]);
        addr += 4u;
    }

    HAL_FLASH_Lock();
}

static void Flash_LoadConfig(void)
{
    const FlashConfig_t *cfg = (const FlashConfig_t *)FLASH_CONFIG_ADDR;

    if (cfg->magic    != FLASH_CONFIG_MAGIC)   return;
    if (cfg->checksum != Config_Checksum(cfg)) return;

    g_freq_mhz  = cfg->freq_mhz;
    g_step_idx  = cfg->step_idx;
    g_duty_mode = (DutyMode_t)cfg->duty_mode;
    g_duty_val  = cfg->duty_val;
    g_brig_mode = (BrigMode_t)cfg->brig_mode;
    g_brig_val  = cfg->brig_val;
    g_running   = cfg->running;

    if (g_freq_mhz < FREQ_MHZ_MIN)  g_freq_mhz = FREQ_MHZ_INIT;
    if (g_freq_mhz > FREQ_MHZ_MAX)  g_freq_mhz = FREQ_MHZ_MAX;
    if (g_step_idx > 2u)             g_step_idx = 0u;
    if (g_duty_val == 0u)            g_duty_val = DUTY_PERC_INIT;
    if (g_brig_val == 0u)            g_brig_val = BRIG_PERC_INIT;
}

/* notification */
static void Notify(const char *msg)
{
    strncpy(notify_msg, msg, sizeof(notify_msg) - 1u);
    notify_msg[sizeof(notify_msg) - 1u] = '\0';
    notify_time = HAL_GetTick();
}

/* button poll */
static void Button_Poll(Button_t *b)
{
    b->pressed = 0;
    b->held    = 0;
    uint32_t now = HAL_GetTick();
    uint8_t  raw = BTN_PRESSED(b->port, b->pin) ? 0u : 1u;

    if (raw == 0u && b->last_state == 1u) {
        if ((now - b->last_time) >= BTN_DEBOUNCE_MS) {
            b->press_time = now;
            b->long_fired = 0u;
        }
    }

    if (raw == 0u && !b->long_fired) {
        if ((now - b->press_time) >= BTN_HOLD_MS) {
            b->held       = 1u;
            b->long_fired = 1u;
        }
    }

    if (raw == 1u && b->last_state == 0u) {
        if (!b->long_fired && (now - b->press_time) >= BTN_DEBOUNCE_MS)
            b->pressed = 1u;
    }

    if (raw != b->last_state)
        b->last_time = now;
    b->last_state = raw;
}

/* encoder processing */
static void Encoder_Process(void)
{
    int32_t diff = EC11_TimerDiff16(&encoder, ENC_READ());
    if (diff == 0) return;
    int32_t before = encoder.step;
    EC11_ProcessTicks(&encoder, diff);
    int32_t delta = encoder.step - before;
    if (delta == 0) return;

    int32_t count = (delta > 0) ? delta : -delta;
    int32_t dir   = (delta > 0) ? 1 : -1;

    switch (g_adj) {
    case ADJ_FREQ: {
        uint32_t step_mhz = g_step_mults[g_step_idx] * FREQ_STEP_BASE_MHZ;
        int32_t  change   = dir * count * (int32_t)step_mhz;
        int32_t  new_mhz  = (int32_t)g_freq_mhz + change;
        if (new_mhz < (int32_t)FREQ_MHZ_MIN) new_mhz = (int32_t)FREQ_MHZ_MIN;
        if (new_mhz > (int32_t)FREQ_MHZ_MAX) new_mhz = (int32_t)FREQ_MHZ_MAX;
        g_freq_mhz = (uint32_t)new_mhz;
        Strobe_ApplyFreq();
        break;
    }
    case ADJ_DUTY: {
        uint32_t mult = g_step_mults[g_step_idx];
        for (int32_t i = 0; i < count; i++)
            for (uint32_t m = 0u; m < mult; m++)
                if (dir > 0) Duty_Increase(); else Duty_Decrease();
        Strobe_ApplyDuty();
        break;
    }
    case ADJ_BRIGHT: {
        uint32_t mult = g_step_mults[g_step_idx];
        for (int32_t i = 0; i < count; i++)
            for (uint32_t m = 0u; m < mult; m++)
                if (dir > 0) Brig_Increase(); else Brig_Decrease();
        break;
    }
    default: break;
    }
}

/* display */
static void Display_Update(void)
{
    SH1106_Fill(SH1106_COLOR_BLACK);

    /* step row -- also used as notification banner */
    if (notify_msg[0] && (HAL_GetTick() - notify_time) < NOTIFY_DURATION_MS) {
        SH1106_WriteStringAt(0, ROW_STEP_Y, notify_msg, Font_8H, SH1106_COLOR_WHITE);
    } else {
        notify_msg[0] = '\0';
        if (g_adj == ADJ_FREQ) {
            static const char * const hz_labels[3] = { "0.1 Hz", "1 Hz", "10 Hz" };
            snprintf(disp_buf, sizeof(disp_buf), "STEP %s", hz_labels[g_step_idx]);
        } else {
            static const char * const step_labels[3] = { "1", "10", "100" };
            snprintf(disp_buf, sizeof(disp_buf), "STEP %s", step_labels[g_step_idx]);
        }
        SH1106_WriteStringAt(0, ROW_STEP_Y, disp_buf, Font_8H, SH1106_COLOR_WHITE);
    }

    /* frequency row */
    char freq_str[10];
    Format_Freq(freq_str, sizeof(freq_str));
    snprintf(disp_buf, sizeof(disp_buf), "%cFreq: %s",
             (g_adj == ADJ_FREQ) ? '>' : ' ', freq_str);
    SH1106_WriteStringAt(0, ROW_FREQ_Y, disp_buf, Font_8H, SH1106_COLOR_WHITE);

    /* duty row */
    snprintf(disp_buf, sizeof(disp_buf), "%cDuty:%2lu%% 1/%lu",
             (g_adj == ADJ_DUTY) ? '>' : ' ',
             Duty_GetPerc(), Duty_GetDivisor());
    SH1106_WriteStringAt(0, ROW_DUTY_Y, disp_buf, Font_8H, SH1106_COLOR_WHITE);

    /* brightness row */
    snprintf(disp_buf, sizeof(disp_buf), "%cBrig:%3lu%% 1/%lu",
             (g_adj == ADJ_BRIGHT) ? '>' : ' ',
             Brig_GetPerc(), Brig_GetDivisor());
    SH1106_WriteStringAt(0, ROW_BRIGHT_Y, disp_buf, Font_8H, SH1106_COLOR_WHITE);

    /* status bar -- inverted */
    SH1106_FillRectangle(0, STATUSBAR_Y0, 127, STATUSBAR_Y1, SH1106_COLOR_WHITE);
    SH1106_WriteStringAt(4, STATUSBAR_TEXT_Y,
        g_running ? "[ ON ] hold=save" : "[OFF]  hold=save",
        Font_8H, SH1106_COLOR_BLACK);

    SH1106_UpdateScreen();
}

/* TIM3_IRQHandler -- strobe timer.
 * Important: comment out HAL_TIM_IRQHandler(&htim3) in stm32f4xx_it.c! */
void TIM3_IRQHandler(void)
{
    if (__HAL_TIM_GET_FLAG(&htim3, TIM_FLAG_UPDATE) &&
        __HAL_TIM_GET_IT_SOURCE(&htim3, TIM_IT_UPDATE))
    {
        __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, Brig_GetCCR());
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_CC1);
        __HAL_TIM_ENABLE_IT(&htim3, TIM_IT_CC1);
    }

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

/* USER CODE END 0 */

/* main */
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

    Flash_LoadConfig();

    HAL_Delay(200);
    if (SH1106_Init() != SH1106_OK)
        while (1) { LED_TOGGLE(); HAL_Delay(200); }

    /* animated splash -- progress bar grows left to right.
     * 20 frames x 20 ms = 400 ms total.
     * right margin of 12 px makes a fully filled bar visually distinct
     * from one that has gone past the edge. */
    {
        const uint8_t  bar_x0    =  4u;
        const uint8_t  bar_x1    = 116u;   /* 12 px right margin */
        const uint8_t  bar_y0    = ROW_DUTY_Y + 1u;
        const uint8_t  bar_y1    = ROW_DUTY_Y + 7u;
        const uint8_t  bar_width = bar_x1 - bar_x0;
        const uint8_t  frames    = 20u;
        uint8_t        f;

        for (f = 0u; f <= frames; f++) {
            SH1106_Fill(SH1106_COLOR_BLACK);
            SH1106_WriteStringAt(16, ROW_FREQ_Y, "STROBE  006",
                                 Font_8H, SH1106_COLOR_WHITE);

            /* outline */
            SH1106_FillRectangle(bar_x0 - 1u, bar_y0 - 1u,
                                 bar_x1 + 1u, bar_y1 + 1u,
                                 SH1106_COLOR_WHITE);
            SH1106_FillRectangle(bar_x0, bar_y0,
                                 bar_x1, bar_y1,
                                 SH1106_COLOR_BLACK);

            /* filled portion */
            uint8_t fill = (uint8_t)((uint16_t)bar_width * f / frames);
            if (fill > 0u) {
                SH1106_FillRectangle(bar_x0, bar_y0,
                                     bar_x0 + fill, bar_y1,
                                     SH1106_COLOR_WHITE);
            }

            SH1106_UpdateScreen();
            HAL_Delay(20);
        }
    }

    EC11_Init(&encoder);
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    ENC_RESET();

    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0u);

    HAL_NVIC_SetPriority(TIM3_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);

    Strobe_ApplyFreq();
    Display_Update();

    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN WHILE */

        Button_Poll(&btn1);
        Button_Poll(&btn2);
        Button_Poll(&btn3);

        /* BTN1 -- encoder push: short=cycle step, hold=save */
        if (btn1.pressed)
            g_step_idx = (uint8_t)((g_step_idx + 1u) % 3u);
        if (btn1.held) {
            Flash_SaveConfig();
            Notify("   SAVED    ");
        }

        /* BTN2 -- top: cycle active parameter */
        if (btn2.pressed)
            g_adj = (AdjMode_t)((g_adj + 1u) % ADJ_COUNT);

        /* BTN3 -- bottom: short=strobe on/off, hold=reset */
        if (btn3.pressed)
            Strobe_SetRunning(g_running ? 0u : 1u);
        if (btn3.held) {
            Apply_Defaults();
            Notify("   RESET    ");
        }

        Encoder_Process();

        uint32_t now = HAL_GetTick();
        if ((now - last_display_tick) >= UPDATE_DELAY_MS) {
            last_display_tick = now;
            Display_Update();
        }

        /* USER CODE END WHILE */
    }
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/* SystemClock_Config.
 * HSE 25 MHz -> PLL (PLLM=12, PLLN=96, PLLP=2) -> SYSCLK 100 MHz.
 * Generated by CubeMX -- preserved here for completeness.
 * If CubeMX regenerates this function, remove the copy below. */
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

/* Error_Handler */
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