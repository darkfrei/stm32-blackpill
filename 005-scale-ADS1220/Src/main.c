/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   ADS1220 Digital Scale
  *
  *  Buttons (all active-low):
  *    PA3 = Confirm
  *    PA4 = Back
  *    PA2 = Encoder Push
  *
  *  MODE: SCALE (default)
  *    Confirm         → Tare
  *    Encoder Push    → enter CALIBRATE mode
  *    Encoder rotate  → (ignored)
  *
  *  MODE: CALIBRATE
  *    Encoder rotate  → divisor ±1 per detent, weight updates live
  *    Confirm         → save divisor to Flash, return to SCALE
  *    Back            → discard changes, return to SCALE
  *
  *  Display top bar always shows current mode:
  *    [ SCALE  ] or [ CALIBRATE ]
  *
  * Flash sector — adjust for your MCU:
  *   STM32F401/F411 (256 KB): FLASH_SECTOR_5  @ 0x08020000
  *   STM32F405/F407 (1 MB):   FLASH_SECTOR_7  @ 0x08060000
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "sh1106.h"
#include "sh1106_fonts.h"
#include "EC11.h"
#include "ads1220.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define UPDATE_DELAY_MS     200

/* Buttons */
#define BTN_CONFIRM_PORT    GPIOA
#define BTN_CONFIRM_PIN     GPIO_PIN_3
#define BTN_BACK_PORT       GPIOA
#define BTN_BACK_PIN        GPIO_PIN_4
#define BTN_PUSH_PORT       GPIOA
#define BTN_PUSH_PIN        GPIO_PIN_2

/* Flash storage configuration - adjust for your device */
#define FLASH_STORAGE_SECTOR    FLASH_SECTOR_5      /* change if needed */
#define FLASH_STORAGE_ADDR      0x08020000U         /* change if needed */
#define FLASH_MAGIC             0xA55A1234U
/* USER CODE END PD */

/* USER CODE BEGIN PM */
#define LED_TOGGLE()             HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13)
/* helper: returns true if pin reads low (active-low button) */
#define BTN_PRESSED(port, pin)   (HAL_GPIO_ReadPin((port), (pin)) == GPIO_PIN_RESET)
#define ENC_READ()               ((uint16_t)(__HAL_TIM_GET_COUNTER(&htim2)))
#define ENC_RESET()              (__HAL_TIM_SET_COUNTER(&htim2, 0))
/* USER CODE END PM */

/* USER CODE BEGIN PTD */
typedef enum {
    MODE_SCALE = 0,
    MODE_CALIBRATE,
} AppMode_t;

/* Structure stored in flash to persist calibration divisor */
typedef struct {
    uint32_t magic;
    int32_t  calibration_divisor;
    uint32_t checksum;
} FlashConfig_t;

/* Simple debounced button — call every main loop iteration
   only basic edge detection implemented (sufficient for ~200 ms update loop) */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       last_raw;     /* raw GPIO state last loop (1=idle,0=pressed) */
    uint8_t       pressed;      /* single-shot flag: just pressed in this loop */
} Button_t;
/* USER CODE END PTD */

/* USER CODE BEGIN PV */
/* Hardware interface objects */
ADS1220_Handle_t hads1220;
EC11_Encoder_t   encoder;

/* Measurement variables */
int32_t  adc_raw            = 0;   /* raw 24-bit ADC value from ADS1220 */
int32_t  adc_code           = 0;   /* adc_raw - tare_offset */
int32_t  weight_grams_x10   = 0;   /* grams * 10 (one decimal digit) */

/* Simple moving average filter for displayed weight */
#define FILTER_SIZE     8
static int32_t  filter_buf[FILTER_SIZE];
static uint8_t  filter_idx  = 0;
static uint8_t  filter_full = 0;
int32_t  weight_filtered    = 0;

/* Timing / counters */
uint32_t last_update        = 0;
uint32_t sample_count       = 0;
uint32_t samples_per_sec    = 0;
uint32_t last_sps_time      = 0;

/* ADS1220 init flag */
uint8_t  ads_init_ok        = 0;

char     display_buf[64];

/* Scale settings */
int32_t  tare_offset            = 0;
int32_t  calibration_divisor    = 1724; /* counts per gram (×10 internal) */
int32_t  cal_divisor_backup     = 1724; /* temporary backup while editing */
uint8_t  tare_pressed           = 0;

/* Application mode */
AppMode_t app_mode = MODE_SCALE;

/* Buttons instances (active-low) */
Button_t btn_confirm = { BTN_CONFIRM_PORT, BTN_CONFIRM_PIN, 1, 0 };
Button_t btn_back    = { BTN_BACK_PORT,    BTN_BACK_PIN,    1, 0 };
Button_t btn_push    = { BTN_PUSH_PORT,    BTN_PUSH_PIN,    1, 0 };

/* Encoder: EC11 driver provides step/position fields */

/* Notification banner (short message displayed at bottom) */
char     notify_msg[20]  = "";
uint32_t notify_time     = 0;
#define  NOTIFY_DURATION_MS  200
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
/* Display update helper */
void     Display_Update(void);

/* show a short message on the bottom line, auto-expire after NOTIFY_DURATION_MS */
void     Notify(const char *msg);

/* poll a simple edge-detect button (active-low) */
static void     Button_Poll(Button_t *b);

/* persistence helpers */
static void     Flash_SaveConfig(void);
static uint8_t  Flash_LoadConfig(void);
static uint32_t Config_Checksum(const FlashConfig_t *c);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
static void    adsCsLow(void);
static void    adsCsHigh(void);
static int     adsSpiTxRx(const uint8_t *tx, uint8_t *rx, uint16_t len);
static uint8_t adsDrdyRead(void);
/* USER CODE END 0 */

/* ============================================================
 *  main
 *  - initialize peripherals and drivers
 *  - run simple main loop: sample ADC, process input, update UI
 * ============================================================ */
int main(void)
{
    HAL_Init();

    /* USER CODE BEGIN Init */
    
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    ITM->LAR = 0xC5ACCE55;  
    ITM->TCR = ITM_TCR_ITMENA_Msk | ITM_TCR_TSENA_Msk;
    ITM->TER = 1;


    /* provide platform callbacks to ADS1220 driver */
    hads1220.csLow    = adsCsLow;
    hads1220.csHigh   = adsCsHigh;
    hads1220.spiTxRx  = adsSpiTxRx;
    hads1220.drdyRead = adsDrdyRead;
    hads1220.gain     = 128;
    hads1220.vref     = 2.048f;
    /* USER CODE END Init */

    SystemClock_Config();

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_TIM2_Init();
    MX_SPI1_Init();

    /* USER CODE BEGIN 2 */
    HAL_Delay(100); /* allow supplies and sensors to settle */

    /* initialize OLED; hang if display not present */
    if (SH1106_Init() != SH1106_OK) {
        while (1) { LED_TOGGLE(); HAL_Delay(200); }
    }
    SH1106_Fill(SH1106_COLOR_BLACK);
    SH1106_UpdateScreen();

    /* encoder hardware + driver */
    EC11_Init(&encoder);
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    ENC_RESET();

    /* initialize ADS1220 ADC */
    if (ADS1220_Init(&hads1220) == ADS1220_OK) {
        ads_init_ok   = 1;
        hads1220.gain = 128; /* confirm gain */
    } else {
        /* show error and halt */
        SH1106_WriteStringAt(10, 28, "ADS1220 INIT FAIL", Font_8H, SH1106_COLOR_WHITE);
        SH1106_UpdateScreen();
        while (1) { LED_TOGGLE(); HAL_Delay(500); }
    }

    /* restore previous calibration (if present) - silent fallback to defaults */
    Flash_LoadConfig();

    last_update   = HAL_GetTick();
    last_sps_time = HAL_GetTick();
    /* USER CODE END 2 */

    /* Main loop */
    while (1)
    {
        /* USER CODE BEGIN 3 */
        uint32_t now = HAL_GetTick();

        /* ---- ADC sampling & basic processing ----
         * Read raw ADC, subtract tare, compute weight (integer math)
         * Apply lightweight moving average for display smoothing.
         */
        if (ads_init_ok) {
            if (ADS1220_ReadData(&hads1220, &adc_raw) == ADS1220_OK) {
                adc_code         = adc_raw - tare_offset;
                weight_grams_x10 = (adc_code * 10) / calibration_divisor;

                /* moving average buffer update */
                filter_buf[filter_idx] = weight_grams_x10;
                filter_idx = (filter_idx + 1) % FILTER_SIZE;
                if (filter_idx == 0) filter_full = 1;

                uint8_t  count = filter_full ? FILTER_SIZE : filter_idx;
                int32_t  sum   = 0;
                for (uint8_t i = 0; i < count; i++) sum += filter_buf[i];
                weight_filtered = sum / (count ? count : 1);

                sample_count++;
            }
        }

        /* update samples per second every 1 second */
        if ((now - last_sps_time) >= 1000) {
            samples_per_sec = sample_count;
            sample_count    = 0;
            last_sps_time   = now;
        }

        /* ---- Input processing: buttons and encoder ---- */
        Button_Poll(&btn_confirm);
        Button_Poll(&btn_back);
        Button_Poll(&btn_push);

        /* encoder delta calculation using EC11 helper (driver provides stable detent counts) */
        int32_t enc_delta = 0;
        {
            int32_t diff = EC11_TimerDiff16(&encoder, ENC_READ());
            if (diff != 0) {
                int32_t before = encoder.step;
                EC11_ProcessTicks(&encoder, diff);
                enc_delta = encoder.step - before; /* logical detent steps */
            }
        }

        /* ---- Application state machine ----
         * SCALE: confirm = tare, push = enter calibrate
         * CALIBRATE: encoder adjusts divisor live, confirm=save, back=undo
         */
        switch (app_mode)
        {
        case MODE_SCALE:
            if (btn_confirm.pressed) {
                /* store current raw as tare reference */
                tare_offset  = adc_raw;
                tare_pressed = 1;
                Notify("Tared");
            }
            if (btn_push.pressed) {
                /* enter calibration; keep backup so Back can restore */
                cal_divisor_backup = calibration_divisor;
                app_mode = MODE_CALIBRATE;
                Notify("CAL");
                /* force immediate UI refresh */
                last_update = 0;
            }
            /* encoder ignored in SCALE mode */
            break;

        case MODE_CALIBRATE:
            if (enc_delta != 0) {
                /* adjust divisor by detents (increase divisor -> less grams) */
                calibration_divisor -= enc_delta;
                if (calibration_divisor <= 0) calibration_divisor = 1;
                /* indicate direction quickly */
                Notify(enc_delta > 0 ? ">" : "<");
                last_update = 0; /* immediate redraw to show feedback */
            }
            if (btn_confirm.pressed) {
                Flash_SaveConfig(); /* persist new divisor */
                app_mode = MODE_SCALE;
                Notify("Saved");
            }
            if (btn_back.pressed) {
                calibration_divisor = cal_divisor_backup; /* restore old value */
                app_mode = MODE_SCALE;
                Notify("Canceled");
            }
            break;
        }

        /* expire short notifications */
        if (notify_msg[0] && (now - notify_time) >= NOTIFY_DURATION_MS) {
            notify_msg[0] = '\0';
        }

        /* ---- Periodic display refresh ---- */
        if ((now - last_update) >= UPDATE_DELAY_MS) {
            last_update = now;
            LED_TOGGLE();
            Display_Update();
        }
        /* USER CODE END 3 */
    }
}

/* ============================================================
 *  Display
 *
 *  Top bar: current mode (inverted background)
 *  Main area: weight, raw adc, net adc, divisor
 *  Bottom: notification or help + SPS
 * ============================================================ */
void Display_Update(void)
{
    SH1106_Fill(SH1106_COLOR_BLACK);

    /* Mode bar: inverted background with centered text */
    SH1106_FillRectangle(0, 0, 127, 11, SH1106_COLOR_WHITE);
    if (app_mode == MODE_SCALE) {
        SH1106_WriteStringAt(26, 2, "   SCALE   ", Font_8H, SH1106_COLOR_BLACK);
    } else {
        SH1106_WriteStringAt(14, 2, "  CALIBRATE  ", Font_8H, SH1106_COLOR_BLACK);
    }

    /* Weight line: show filtered value if tare performed */
    if (tare_pressed) {
        int32_t g = weight_filtered / 10;
        int32_t d = (weight_filtered < 0) ? -(weight_filtered % 10) : weight_filtered % 10;
        snprintf(display_buf, sizeof(display_buf), "Weight: %ld.%ld g", g, d);
    } else {
        snprintf(display_buf, sizeof(display_buf), "Weight: -- tare --");
    }
    SH1106_WriteStringAt(2, 13, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* Raw ADC and net ADC */
    snprintf(display_buf, sizeof(display_buf), "RAW: %ld", adc_raw);
    SH1106_WriteStringAt(2, 23, display_buf, Font_8H, SH1106_COLOR_WHITE);

    snprintf(display_buf, sizeof(display_buf), "ADC: %ld", adc_code);
    SH1106_WriteStringAt(2, 33, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* Divisor display (calibration parameter) */
    snprintf(display_buf, sizeof(display_buf), "DIV: %ld", calibration_divisor);
    SH1106_WriteStringAt(2, 43, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* Bottom line: either notification centered, or context hint + SPS */
    if (notify_msg[0]) {
        uint8_t len = (uint8_t)strlen(notify_msg);
        uint8_t x   = (128 - len * 8) / 2;
        if (x > 127) x = 0;
        SH1106_FillRectangle(0, 51, 127, 63, SH1106_COLOR_WHITE);
        SH1106_WriteStringAt(x, 53, notify_msg, Font_8H, SH1106_COLOR_BLACK);
    } else if (app_mode == MODE_SCALE) {
        snprintf(display_buf, sizeof(display_buf), "OK=tare push=cal %lu", samples_per_sec);
        SH1106_WriteStringAt(2, 53, display_buf, Font_8H, SH1106_COLOR_WHITE);
    } else {
        snprintf(display_buf, sizeof(display_buf), "OK=save back=undo %lu", samples_per_sec);
        SH1106_WriteStringAt(2, 53, display_buf, Font_8H, SH1106_COLOR_WHITE);
    }

    SH1106_UpdateScreen();
}

/* show a short centered message on the bottom line */
void Notify(const char *msg)
{
    strncpy(notify_msg, msg, sizeof(notify_msg) - 1);
    notify_msg[sizeof(notify_msg) - 1] = '\0';
    notify_time = HAL_GetTick();
}

/* ============================================================
 *  Button poll (edge detect)
 *  - Active-low buttons.
 *  - Simple edge detection suffices at ~200 ms update rate.
 * ============================================================ */
static void Button_Poll(Button_t *b)
{
    b->pressed      = 0;
    /* raw logic: treat 0 when pressed, 1 when idle to simplify edge check */
    uint8_t raw     = BTN_PRESSED(b->port, b->pin) ? 0 : 1;
    if (raw == 0 && b->last_raw == 1) { /* falling edge: newly pressed */
        b->pressed = 1;
    }
    b->last_raw = raw;
}

/* ============================================================
 *  Flash persistence helpers
 *  - store only a small FlashConfig_t containing magic/checksum/divisor
 *  - caller must ensure sector/address match MCU layout
 * ============================================================ */
static uint32_t Config_Checksum(const FlashConfig_t *c)
{
    return c->magic ^ (uint32_t)c->calibration_divisor;
}

static void Flash_SaveConfig(void)
{
    FlashConfig_t cfg;
    cfg.magic               = FLASH_MAGIC;
    cfg.calibration_divisor = calibration_divisor;
    cfg.checksum            = Config_Checksum(&cfg);

    HAL_FLASH_Unlock();

    /* erase one sector (Flash layout dependent) */
    FLASH_EraseInitTypeDef erase = {0};
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.Sector       = FLASH_STORAGE_SECTOR;
    erase.NbSectors    = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3; /* 2.7V - 3.6V devices */

    uint32_t err = 0;
    HAL_FLASHEx_Erase(&erase, &err);

    /* program words sequentially */
    uint32_t addr  = FLASH_STORAGE_ADDR;
    uint32_t *data = (uint32_t *)&cfg;
    for (uint32_t i = 0; i < sizeof(FlashConfig_t) / 4; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data[i]);
        addr += 4;
    }

    HAL_FLASH_Lock();
}

/* Load config from flash, return 1 if valid and applied, 0 otherwise */
static uint8_t Flash_LoadConfig(void)
{
    const FlashConfig_t *cfg = (const FlashConfig_t *)FLASH_STORAGE_ADDR;
    if (cfg->magic    != FLASH_MAGIC)          return 0;
    if (cfg->checksum != Config_Checksum(cfg)) return 0;
    if (cfg->calibration_divisor == 0)         return 0;
    calibration_divisor = cfg->calibration_divisor;
    return 1;
}

/* ============================================================
 *  ADS1220 platform callbacks (SPI + CS + DRDY)
 *  - These adapt the ADS1220 driver to the HAL SPI + GPIOs used
 * ============================================================ */
static void adsCsLow(void)  { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); }
static void adsCsHigh(void) { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);   }

static int adsSpiTxRx(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    HAL_StatusTypeDef st;
    if (tx && rx) {
        st = HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)tx, rx, len, 100);
    } else if (rx) {
        static uint8_t zeros[256];
        if (len > sizeof(zeros)) return -1;
        st = HAL_SPI_TransmitReceive(&hspi1, zeros, rx, len, 100);
    } else if (tx) {
        st = HAL_SPI_Transmit(&hspi1, (uint8_t *)tx, len, 100);
    } else {
        return -1;
    }
    return (st == HAL_OK) ? 0 : -1;
}

/* DRDY line is active-low on PB1 in the CubeMX configuration */
static uint8_t adsDrdyRead(void)
{
    return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET;
}

/* ============================================================
 *  System Clock (CubeMX generated values retained)
 * ============================================================ */
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

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif