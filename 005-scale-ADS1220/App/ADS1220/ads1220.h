/**
  ******************************************************************************
  * @file    ads1220.h
  * @brief   ADS1220 24-bit ADC Driver Header
  ******************************************************************************
  */

#ifndef ADS1220_H
#define ADS1220_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ADS1220 Register Addresses */
#define ADS1220_REG_CONFIG0     0x00
#define ADS1220_REG_CONFIG1     0x01
#define ADS1220_REG_CONFIG2     0x02
#define ADS1220_REG_CONFIG3     0x03

/* ADS1220 Commands */
#define ADS1220_CMD_RESET       0x06
#define ADS1220_CMD_START       0x08
#define ADS1220_CMD_POWERDOWN   0x02
#define ADS1220_CMD_RDATA       0x10
#define ADS1220_CMD_RREG        0x20
#define ADS1220_CMD_WREG        0x40

/* Configuration Register 0 Bits */
#define ADS1220_MUX_AIN0_AIN1   0x00
#define ADS1220_MUX_AIN0_AIN2   0x10
#define ADS1220_MUX_AIN0_AIN3   0x20
#define ADS1220_MUX_AIN1_AIN2   0x30
#define ADS1220_MUX_AIN1_AIN3   0x40
#define ADS1220_MUX_AIN2_AIN3   0x50
#define ADS1220_MUX_AIN1_AIN0   0x60
#define ADS1220_MUX_AIN3_AIN2   0x70

#define ADS1220_GAIN_1          0x00
#define ADS1220_GAIN_2          0x02
#define ADS1220_GAIN_4          0x04
#define ADS1220_GAIN_8          0x06
#define ADS1220_GAIN_16         0x08
#define ADS1220_GAIN_32         0x0A
#define ADS1220_GAIN_64         0x0C
#define ADS1220_GAIN_128        0x0E

#define ADS1220_PGA_BYPASS      0x01

/* Configuration Register 1 Bits */
#define ADS1220_DR_20SPS        0x00
#define ADS1220_DR_45SPS        0x20
#define ADS1220_DR_90SPS        0x40
#define ADS1220_DR_175SPS       0x60
#define ADS1220_DR_330SPS       0x80
#define ADS1220_DR_600SPS       0xA0
#define ADS1220_DR_1000SPS      0xC0

#define ADS1220_MODE_NORMAL     0x00
#define ADS1220_MODE_DUTY       0x08
#define ADS1220_MODE_TURBO      0x10

#define ADS1220_CM_SINGLE       0x04
#define ADS1220_TS_DISABLED     0x00
#define ADS1220_BCS_OFF         0x00

/* Configuration Register 2 Bits */
#define ADS1220_VREF_INTERNAL   0x00
#define ADS1220_VREF_EXT_REF0   0x40
#define ADS1220_VREF_EXT_REF1   0x80
#define ADS1220_VREF_SUPPLY     0xC0

#define ADS1220_50HZ_60HZ_OFF   0x00
#define ADS1220_IDAC_OFF        0x00
#define ADS1220_IDAC_10UA       0x01
#define ADS1220_IDAC_50UA       0x02
#define ADS1220_IDAC_100UA      0x03

/* Status */
typedef enum {
    ADS1220_OK = 0,
    ADS1220_ERROR = 1,
    ADS1220_TIMEOUT = 2
} ADS1220_Status_t;

/* ADS1220 Handle Structure */
typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *drdy_port;
    uint16_t drdy_pin;
    uint8_t gain;
    float vref;
} ADS1220_Handle_t;

/* Function Prototypes */
ADS1220_Status_t ADS1220_Init(ADS1220_Handle_t *hads);
ADS1220_Status_t ADS1220_Reset(ADS1220_Handle_t *hads);
ADS1220_Status_t ADS1220_WriteRegister(ADS1220_Handle_t *hads, uint8_t reg, uint8_t value);
ADS1220_Status_t ADS1220_ReadRegister(ADS1220_Handle_t *hads, uint8_t reg, uint8_t *value);
ADS1220_Status_t ADS1220_ReadData(ADS1220_Handle_t *hads, int32_t *data);
uint8_t ADS1220_DataReady(ADS1220_Handle_t *hads);
float ADS1220_CodeToVoltage(ADS1220_Handle_t *hads, int32_t code);

#ifdef __cplusplus
}
#endif

#endif /* ADS1220_H */