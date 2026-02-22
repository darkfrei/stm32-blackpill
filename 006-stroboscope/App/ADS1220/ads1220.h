#ifndef __ADS1220_H__
#define __ADS1220_H__

#include <stdint.h>

/* Status */
typedef enum {
    ADS1220_OK = 0,
    ADS1220_ERROR = -1,
    ADS1220_TIMEOUT = -2
} ADS1220_Status_t;

/* Commands */
#define ADS1220_CMD_RESET   0x06
#define ADS1220_CMD_START   0x08
#define ADS1220_CMD_STOP    0x0A
#define ADS1220_CMD_RDATA   0x10
#define ADS1220_CMD_RREG    0x20
#define ADS1220_CMD_WREG    0x40

/* Registers (addresses) */
#define ADS1220_REG_CONFIG0 0x00
#define ADS1220_REG_CONFIG1 0x01
#define ADS1220_REG_CONFIG2 0x02
#define ADS1220_REG_CONFIG3 0x03

/* Useful field masks (you can expand as needed) */
#define ADS1220_MUX_AIN0_AIN1   (0x00 << 4)
#define ADS1220_MUX_AIN1_AIN0   (0x06 << 4)
#define ADS1220_GAIN_128        (0x07 << 1)

#define ADS1220_DR_20SPS        (0x00 << 5)
#define ADS1220_MODE_NORMAL     (0x00 << 3)
#define ADS1220_CM_CONTINUOUS   (0x01 << 2)

#define ADS1220_VREF_INTERNAL   (0x00)
#define ADS1220_50HZ_60HZ_OFF   (0x00)
#define ADS1220_IDAC_OFF        (0x00)

/* Handle with callbacks - HAL-free core driver */
typedef struct {
    /* Low level callbacks - must be provided by application/main.c */
    void     (*csLow)(void);
    void     (*csHigh)(void);
    /* spiTxRx: if tx != NULL and rx != NULL -> full duplex
       if tx != NULL and rx == NULL -> transmit only
       if tx == NULL and rx != NULL -> receive only (send 0x00s)
       should return 0 on success, non-zero on error */
    int      (*spiTxRx)(const uint8_t *tx, uint8_t *rx, uint16_t len);
    uint8_t  (*drdyRead)(void); /* return 1 if DRDY (data ready), 0 otherwise */

    /* Optional configuration state filled by init */
    uint16_t gain;    /* 1,2,4,...128 */
    float    vref;    /* volts, e.g. 2.048f */
} ADS1220_Handle_t;

/* API */
ADS1220_Status_t ADS1220_Init(ADS1220_Handle_t *hads);
ADS1220_Status_t ADS1220_Reset(ADS1220_Handle_t *hads);
ADS1220_Status_t ADS1220_SendCommand(ADS1220_Handle_t *hads, uint8_t cmd);
ADS1220_Status_t ADS1220_WriteRegister(ADS1220_Handle_t *hads, uint8_t reg, uint8_t value);
ADS1220_Status_t ADS1220_ReadRegister(ADS1220_Handle_t *hads, uint8_t reg, uint8_t *value);
uint8_t          ADS1220_DataReady(ADS1220_Handle_t *hads);
ADS1220_Status_t ADS1220_ReadData(ADS1220_Handle_t *hads, int32_t *data);
float            ADS1220_CodeToVoltage(ADS1220_Handle_t *hads, int32_t code);

#endif /* __ADS1220_H__ */
