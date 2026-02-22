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

/* Flash */
#define FLASH_STORAGE_SECTOR    FLASH_SECTOR_5      /* change if needed */
#define FLASH_STORAGE_ADDR      0x08020000U         /* change if needed */
#define FLASH_MAGIC             0xA55A1234U
/* USER CODE END PD */

/* USER CODE BEGIN PM */
#define LED_TOGGLE()  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13)
#define BTN_PRESSED(port, pin)  (HAL_GPIO_ReadPin((port), (pin)) == GPIO_PIN_RESET)
/* USER CODE END PM */

/* USER CODE BEGIN PTD */
typedef enum {
    MODE_SCALE = 0,
    MODE_CALIBRATE,
} AppMode_t;

typedef struct {
    uint32_t magic;
    int32_t  calibration_divisor;
    uint32_t checksum;
} FlashConfig_t;

/* Simple debounced button — call every main loop iteration */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       last_raw;     /* raw GPIO state last loop          */
    uint8_t       pressed;      /* single-shot flag: just pressed    */
} Button_t;
/* USER CODE END PTD */

/* USER CODE BEGIN PV */
ADS1220_Handle_t hads1220;
EC11_Encoder_t   encoder;

int32_t  adc_raw            = 0;
int32_t  adc_code           = 0;
int32_t  weight_grams_x10   = 0;

uint32_t last_update        = 0;
uint32_t sample_count       = 0;
uint32_t samples_per_sec    = 0;
uint32_t last_sps_time      = 0;

uint8_t  ads_init_ok        = 0;
uint8_t  reg_check[4]       = {0};
char     display_buf[64];

/* Scale */
int32_t  tare_offset            = 0;
int32_t  calibration_divisor    = 1724;
int32_t  cal_divisor_backup     = 1724;  /* restored if Back is pressed */
uint8_t  tare_pressed           = 0;

/* Mode */
AppMode_t app_mode = MODE_SCALE;

/* Buttons */
Button_t btn_confirm = { BTN_CONFIRM_PORT, BTN_CONFIRM_PIN, 1, 0 };
Button_t btn_back    = { BTN_BACK_PORT,    BTN_BACK_PIN,    1, 0 };
Button_t btn_push    = { BTN_PUSH_PORT,    BTN_PUSH_PIN,    1, 0 };

/* Encoder */
int16_t  enc_prev_cnt = 0;

/* Notification banner */
uint8_t  notify_msg[20]  = "";
uint32_t notify_time     = 0;
#define  NOTIFY_DURATION_MS  1200
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
void     Display_Update(void);
void     Notify(const char *msg);
static void     Button_Poll(Button_t *b);
static int32_t  Encoder_GetDelta(void);
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
 * ============================================================ */
int main(void)
{
    HAL_Init();

    /* USER CODE BEGIN Init */
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
    HAL_Delay(100);

    if (SH1106_Init() != SH1106_OK) {
        while (1) { LED_TOGGLE(); HAL_Delay(200); }
    }
    SH1106_Fill(SH1106_COLOR_BLACK);
    SH1106_UpdateScreen();

    EC11_Init(&encoder);
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    enc_prev_cnt = (int16_t)TIM2->CNT;

    if (ADS1220_Init(&hads1220) == ADS1220_OK) {
        ads_init_ok   = 1;
        hads1220.gain = 128;
    } else {
        SH1106_WriteStringAt(10, 28, "ADS1220 INIT FAIL", Font_8H, SH1106_COLOR_WHITE);
        SH1106_UpdateScreen();
        while (1) { LED_TOGGLE(); HAL_Delay(500); }
    }

    ADS1220_ReadRegister(&hads1220, 0, &reg_check[0]);
    ADS1220_ReadRegister(&hads1220, 1, &reg_check[1]);
    ADS1220_ReadRegister(&hads1220, 2, &reg_check[2]);
    ADS1220_ReadRegister(&hads1220, 3, &reg_check[3]);

    Flash_LoadConfig();   /* restore divisor; silently keeps default if blank */

    last_update   = HAL_GetTick();
    last_sps_time = HAL_GetTick();
    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN 3 */
        uint32_t now = HAL_GetTick();

        /* ---- ADC ------------------------------------------------ */
        if (ads_init_ok) {
            if (ADS1220_ReadData(&hads1220, &adc_raw) == ADS1220_OK) {
                adc_code         = adc_raw - tare_offset;
                weight_grams_x10 = (adc_code * 10) / calibration_divisor;
                sample_count++;
            }
        }
        if ((now - last_sps_time) >= 1000) {
            samples_per_sec = sample_count;
            sample_count    = 0;
            last_sps_time   = now;
        }

        /* ---- Buttons -------------------------------------------- */
        Button_Poll(&btn_confirm);
        Button_Poll(&btn_back);
        Button_Poll(&btn_push);

        /* ---- Encoder delta -------------------------------------- */
        int32_t enc_delta = Encoder_GetDelta();

        /* ---- State machine -------------------------------------- */
        switch (app_mode)
        {
        case MODE_SCALE:
            if (btn_confirm.pressed) {
                tare_offset  = adc_raw;
                tare_pressed = 1;
                Notify("Tared");
            }
            if (btn_push.pressed) {
                cal_divisor_backup  = calibration_divisor; /* save for Back */
                app_mode = MODE_CALIBRATE;
            }
            /* encoder ignored in SCALE mode */
            break;

        case MODE_CALIBRATE:
            /* Encoder rotates divisor live */
            if (enc_delta != 0) {
                calibration_divisor += enc_delta;
                if (calibration_divisor == 0) calibration_divisor = 1;
            }
            /* Confirm → save to Flash, return */
            if (btn_confirm.pressed) {
                Flash_SaveConfig();
                app_mode = MODE_SCALE;
                Notify("Saved to Flash");
            }
            /* Back → discard, restore old divisor, return */
            if (btn_back.pressed) {
                calibration_divisor = cal_divisor_backup;
                app_mode = MODE_SCALE;
                Notify("Cancelled");
            }
            break;
        }

        /* ---- Expire notification banner ------------------------- */
        if (notify_msg[0] && (now - notify_time) >= NOTIFY_DURATION_MS) {
            notify_msg[0] = '\0';
        }

        /* ---- Periodic display ----------------------------------- */
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
 *  Layout (128 x 64):
 *  ┌──────────────────────────────┐  y=0
 *  │  [   SCALE   ] / [CALIBRATE]│  y=0..11  ← mode bar (inverted)
 *  ├──────────────────────────────┤  y=12
 *  │  Weight: xxx.x g             │  y=13
 *  │  RAW:  xxxxxxxxx             │  y=23
 *  │  ADC:  xxxxxxxxx             │  y=33
 *  │  DIV:  xxxxx  SPS:xxx        │  y=43
 *  │  <notification or hint>      │  y=53
 *  └──────────────────────────────┘
 * ============================================================ */
void Display_Update(void)
{
    SH1106_Fill(SH1106_COLOR_BLACK);

    /* ---- Mode bar (inverted background) ---- */
    SH1106_FillRectangle(0, 0, 127, 11, SH1106_COLOR_WHITE);
    if (app_mode == MODE_SCALE) {
        SH1106_WriteStringAt(26, 2, "   SCALE   ", Font_8H, SH1106_COLOR_BLACK);
    } else {
        SH1106_WriteStringAt(14, 2, "  CALIBRATE  ", Font_8H, SH1106_COLOR_BLACK);
    }

    /* ---- Weight ---- */
    if (tare_pressed) {
        int32_t g = weight_grams_x10 / 10;
        int32_t d = (weight_grams_x10 < 0) ? -(weight_grams_x10 % 10)
                                            :   weight_grams_x10 % 10;
        snprintf(display_buf, sizeof(display_buf), "Weight: %ld.%ld g", g, d);
    } else {
        snprintf(display_buf, sizeof(display_buf), "Weight: -- tare --");
    }
    SH1106_WriteStringAt(2, 13, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* ---- Raw ADC ---- */
    snprintf(display_buf, sizeof(display_buf), "RAW: %ld", adc_raw);
    SH1106_WriteStringAt(2, 23, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* ---- Net ADC ---- */
    snprintf(display_buf, sizeof(display_buf), "ADC: %ld", adc_code);
    SH1106_WriteStringAt(2, 33, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* ---- Divisor + SPS ---- */
    snprintf(display_buf, sizeof(display_buf),
             "DIV:%ld SPS:%lu", calibration_divisor, samples_per_sec);
    SH1106_WriteStringAt(2, 43, display_buf, Font_8H, SH1106_COLOR_WHITE);

    /* ---- Bottom line: notification OR context hint ---- */
    if (notify_msg[0]) {
        SH1106_WriteStringAt(2, 53, (char *)notify_msg, Font_8H, SH1106_COLOR_WHITE);
    } else if (app_mode == MODE_SCALE) {
        SH1106_WriteStringAt(2, 53, "OK=tare  push=cal", Font_8H, SH1106_COLOR_WHITE);
    } else {
        SH1106_WriteStringAt(2, 53, "OK=save  back=undo", Font_8H, SH1106_COLOR_WHITE);
    }

    SH1106_UpdateScreen();
}

void Notify(const char *msg)
{
    strncpy((char *)notify_msg, msg, sizeof(notify_msg) - 1);
    notify_msg[sizeof(notify_msg) - 1] = '\0';
    notify_time = HAL_GetTick();
}

/* ============================================================
 *  Button poll  (call every loop, active-low, no debounce timer
 *  needed at 200 Hz loop rate — just edge detection)
 * ============================================================ */
static void Button_Poll(Button_t *b)
{
    b->pressed      = 0;
    uint8_t raw     = BTN_PRESSED(b->port, b->pin) ? 0 : 1; /* 0=pressed,1=idle */
    if (raw == 0 && b->last_raw == 1) {                      /* falling edge */
        b->pressed = 1;
    }
    b->last_raw = raw;
}

/* ============================================================
 *  Encoder  (TIM2 encoder mode, 2 counts per detent)
 * ============================================================ */
static int32_t Encoder_GetDelta(void)
{
    int16_t cnt   = (int16_t)TIM2->CNT;
    int16_t delta = cnt - enc_prev_cnt;
    enc_prev_cnt  = cnt;
    return (int32_t)(delta / 2);
}

/* ============================================================
 *  Flash persistent storage
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

    FLASH_EraseInitTypeDef erase = {0};
    erase.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase.Sector       = FLASH_STORAGE_SECTOR;
    erase.NbSectors    = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    uint32_t err = 0;
    HAL_FLASHEx_Erase(&erase, &err);

    uint32_t  addr = FLASH_STORAGE_ADDR;
    uint32_t *data = (uint32_t *)&cfg;
    for (uint32_t i = 0; i < sizeof(FlashConfig_t) / 4; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data[i]);
        addr += 4;
    }

    HAL_FLASH_Lock();
}

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
 *  ADS1220 platform callbacks
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

static uint8_t adsDrdyRead(void)
{
    return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET;
}

/* ============================================================
 *  System Clock
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
