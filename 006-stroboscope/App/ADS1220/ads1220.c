/**
  ******************************************************************************
  * @file    ads1220.c
  * @brief   ADS1220 driver (HAL-free core, uses callbacks provided by app)
  ******************************************************************************
  */

#include "ads1220.h"
#include <string.h>

#define ADS1220_READ_TIMEOUT_MS 100

/* Internal helper: send bytes via callback (tx may be NULL, rx may be NULL) */
static int ads_spi_xfer(ADS1220_Handle_t *h, const uint8_t *tx, uint8_t *rx, uint16_t len) {
    if (!h || !h->spiTxRx) return -1;
    return h->spiTxRx(tx, rx, len);
}

ADS1220_Status_t ADS1220_SendCommand(ADS1220_Handle_t *hads, uint8_t cmd)
{
    if (!hads || !hads->csLow || !hads->csHigh || !hads->spiTxRx) return ADS1220_ERROR;

    hads->csLow();
    int r = ads_spi_xfer(hads, &cmd, NULL, 1);
    hads->csHigh();

    return (r == 0) ? ADS1220_OK : ADS1220_ERROR;
}

ADS1220_Status_t ADS1220_Reset(ADS1220_Handle_t *hads)
{
    return ADS1220_SendCommand(hads, ADS1220_CMD_RESET);
}

ADS1220_Status_t ADS1220_WriteRegister(ADS1220_Handle_t *hads, uint8_t reg, uint8_t value)
{
    if (!hads || !hads->csLow || !hads->csHigh || !hads->spiTxRx) return ADS1220_ERROR;

    uint8_t cmd[2];
    cmd[0] = ADS1220_CMD_WREG | ((reg & 0x03) << 2); /* write starting at reg, nn=0 => write 1 byte */
    cmd[1] = value;

    hads->csLow();
    int r = ads_spi_xfer(hads, cmd, NULL, 2);
    hads->csHigh();

    return (r == 0) ? ADS1220_OK : ADS1220_ERROR;
}

ADS1220_Status_t ADS1220_ReadRegister(ADS1220_Handle_t *hads, uint8_t reg, uint8_t *value)
{
    if (!hads || !hads->csLow || !hads->csHigh || !hads->spiTxRx || !value) return ADS1220_ERROR;

    uint8_t cmd = ADS1220_CMD_RREG | ((reg & 0x03) << 2);

    hads->csLow();
    int r = ads_spi_xfer(hads, &cmd, NULL, 1);
    if (r == 0) {
        r = ads_spi_xfer(hads, NULL, value, 1); /* read one byte */
    }
    hads->csHigh();

    return (r == 0) ? ADS1220_OK : ADS1220_ERROR;
}

uint8_t ADS1220_DataReady(ADS1220_Handle_t *hads)
{
    if (!hads || !hads->drdyRead) return 0;
    return hads->drdyRead() ? 1 : 0;
}

/* Read 3 bytes (24-bit signed). In continuous mode, read only when DRDY=1. */
ADS1220_Status_t ADS1220_ReadData(ADS1220_Handle_t *hads, int32_t *data)
{
    if (!hads || !data || !hads->csLow || !hads->csHigh || !hads->spiTxRx) return ADS1220_ERROR;

    /* Check DRDY first */
    if (!ADS1220_DataReady(hads)) {
        return ADS1220_TIMEOUT;
    }

    uint8_t rx[3] = {0};

    hads->csLow();
    int r = ads_spi_xfer(hads, NULL, rx, 3); /* receive 3 bytes (sends 0x00 internally in callback if needed) */
    hads->csHigh();

    if (r != 0) return ADS1220_ERROR;

    int32_t raw = ((int32_t)rx[0] << 16) | ((int32_t)rx[1] << 8) | (int32_t)rx[2];
    if (raw & 0x800000) raw |= 0xFF000000; /* sign extend */

    *data = raw;
    return ADS1220_OK;
}

ADS1220_Status_t ADS1220_Init(ADS1220_Handle_t *hads)
{
    if (!hads) return ADS1220_ERROR;

    /* Minimal checks: callbacks must be set by application */
    if (!hads->csLow || !hads->csHigh || !hads->spiTxRx || !hads->drdyRead) {
        return ADS1220_ERROR;
    }

    /* Reset device */
    if (ADS1220_Reset(hads) != ADS1220_OK) return ADS1220_ERROR;

    /* Small delay: caller must provide hardware delay before calling Init if needed */

    /* Configure register0: MUX AIN1/AIN0, GAIN=128, PGA enabled */
    if (ADS1220_WriteRegister(hads, ADS1220_REG_CONFIG0, (uint8_t)(ADS1220_MUX_AIN1_AIN0 | ADS1220_GAIN_128)) != ADS1220_OK) {
        return ADS1220_ERROR;
    }

    /* Configure register1: 20SPS, Normal mode, Continuous conversion */
    if (ADS1220_WriteRegister(hads, ADS1220_REG_CONFIG1, (uint8_t)(ADS1220_DR_20SPS | ADS1220_MODE_NORMAL | ADS1220_CM_CONTINUOUS)) != ADS1220_OK) {
        return ADS1220_ERROR;
    }

    /* Configure register2: internal ref, no 50/60 rejection */
    if (ADS1220_WriteRegister(hads, ADS1220_REG_CONFIG2, (uint8_t)(ADS1220_VREF_INTERNAL | ADS1220_50HZ_60HZ_OFF)) != ADS1220_OK) {
        return ADS1220_ERROR;
    }

    /* Configure register3: idac off */
    if (ADS1220_WriteRegister(hads, ADS1220_REG_CONFIG3, (uint8_t)ADS1220_IDAC_OFF) != ADS1220_OK) {
        return ADS1220_ERROR;
    }

    /* store defaults if not set */
    if (hads->gain == 0) hads->gain = 128;
    if (hads->vref == 0.0f) hads->vref = 2.048f;

    /* Start continuous conversions */
    if (ADS1220_SendCommand(hads, ADS1220_CMD_START) != ADS1220_OK) {
        return ADS1220_ERROR;
    }

    return ADS1220_OK;
}

float ADS1220_CodeToVoltage(ADS1220_Handle_t *hads, int32_t code)
{
    if (!hads || hads->gain == 0) return 0.0f;

    /* ADS1220: counts ±2^23, full-scale = Vref / gain */
    const float denom = 8388608.0f; /* 2^23 */
    float lsb = (hads->vref / (float)hads->gain) / denom; /* result in volts per count */
    return (float)code * lsb;
}
