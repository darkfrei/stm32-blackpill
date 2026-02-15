/**
  ******************************************************************************
  * @file    ads1220.c
  * @brief   ADS1220 24-bit ADC Driver Implementation
  ******************************************************************************
  */

#include "ads1220.h"

/* Private defines */
#define ADS1220_TIMEOUT         100
#define ADS1220_CS_LOW()        HAL_GPIO_WritePin(hads->cs_port, hads->cs_pin, GPIO_PIN_RESET)
#define ADS1220_CS_HIGH()       HAL_GPIO_WritePin(hads->cs_port, hads->cs_pin, GPIO_PIN_SET)

/* Private functions */
static ADS1220_Status_t ADS1220_SendCommand(ADS1220_Handle_t *hads, uint8_t cmd);

/**
  * @brief  Initialize ADS1220
  * @param  hads: Pointer to ADS1220 handle
  * @retval Status
  */
ADS1220_Status_t ADS1220_Init(ADS1220_Handle_t *hads)
{
    if (hads == NULL) {
        return ADS1220_ERROR;
    }

    /* Ensure CS is high */
    ADS1220_CS_HIGH();
    HAL_Delay(1);

    /* Reset ADS1220 */
    if (ADS1220_Reset(hads) != ADS1220_OK) {
        return ADS1220_ERROR;
    }

    HAL_Delay(50);

    // /* Configure Register 0: MUX=AIN0/AIN1, GAIN=128, PGA enabled */
    // if (ADS1220_WriteRegister(hads, ADS1220_REG_CONFIG0, 
        // ADS1220_MUX_AIN0_AIN1 | ADS1220_GAIN_128) != ADS1220_OK) {
        // return ADS1220_ERROR;
    // }
    
    /* Configure Register 0: MUX=AIN1/AIN0 (inverted), GAIN=128, PGA enabled */
    if (ADS1220_WriteRegister(hads, ADS1220_REG_CONFIG0, 
        ADS1220_MUX_AIN1_AIN0 | ADS1220_GAIN_128) != ADS1220_OK) {
        return ADS1220_ERROR;
    }

    // /* Configure Register 1: 20 SPS, Normal mode, Single-shot */
    // if (ADS1220_WriteRegister(hads, ADS1220_REG_CONFIG1,
        // ADS1220_DR_20SPS | ADS1220_MODE_NORMAL | ADS1220_CM_SINGLE) != ADS1220_OK) {
        // return ADS1220_ERROR;
    // }
    
        /* Configure Register 1: 20 SPS, Normal mode, Continuous */
    if (ADS1220_WriteRegister(hads, ADS1220_REG_CONFIG1,
        ADS1220_DR_20SPS | ADS1220_MODE_NORMAL) != ADS1220_OK) {
        return ADS1220_ERROR;
    }

    /* Configure Register 2: Internal 2.048V reference, no 50/60Hz rejection */
    if (ADS1220_WriteRegister(hads, ADS1220_REG_CONFIG2,
        ADS1220_VREF_INTERNAL | ADS1220_50HZ_60HZ_OFF) != ADS1220_OK) {
        return ADS1220_ERROR;
    }

    /* Configure Register 3: No IDAC */
    if (ADS1220_WriteRegister(hads, ADS1220_REG_CONFIG3,
        ADS1220_IDAC_OFF) != ADS1220_OK) {
        return ADS1220_ERROR;
    }

    /* Set gain and vref in handle */
    hads->gain = 128;
    hads->vref = 2.048f;
    
        /* Start continuous conversion */
    if (ADS1220_SendCommand(hads, ADS1220_CMD_START) != ADS1220_OK) {
        return ADS1220_ERROR;
    }

    return ADS1220_OK;
}

/**
  * @brief  Reset ADS1220
  * @param  hads: Pointer to ADS1220 handle
  * @retval Status
  */
ADS1220_Status_t ADS1220_Reset(ADS1220_Handle_t *hads)
{
    return ADS1220_SendCommand(hads, ADS1220_CMD_RESET);
}

/**
  * @brief  Send command to ADS1220
  * @param  hads: Pointer to ADS1220 handle
  * @param  cmd: Command byte
  * @retval Status
  */
static ADS1220_Status_t ADS1220_SendCommand(ADS1220_Handle_t *hads, uint8_t cmd)
{
    HAL_StatusTypeDef status;

    ADS1220_CS_LOW();
    status = HAL_SPI_Transmit(hads->hspi, &cmd, 1, ADS1220_TIMEOUT);
    ADS1220_CS_HIGH();

    return (status == HAL_OK) ? ADS1220_OK : ADS1220_ERROR;
}

/**
  * @brief  Write to ADS1220 register
  * @param  hads: Pointer to ADS1220 handle
  * @param  reg: Register address (0-3)
  * @param  value: Value to write
  * @retval Status
  */
ADS1220_Status_t ADS1220_WriteRegister(ADS1220_Handle_t *hads, uint8_t reg, uint8_t value)
{
    uint8_t cmd[2];
    HAL_StatusTypeDef status;

    cmd[0] = ADS1220_CMD_WREG | (reg << 2);
    cmd[1] = value;

    ADS1220_CS_LOW();
    status = HAL_SPI_Transmit(hads->hspi, cmd, 2, ADS1220_TIMEOUT);
    ADS1220_CS_HIGH();

    return (status == HAL_OK) ? ADS1220_OK : ADS1220_ERROR;
}

/**
  * @brief  Read from ADS1220 register
  * @param  hads: Pointer to ADS1220 handle
  * @param  reg: Register address (0-3)
  * @param  value: Pointer to store read value
  * @retval Status
  */
ADS1220_Status_t ADS1220_ReadRegister(ADS1220_Handle_t *hads, uint8_t reg, uint8_t *value)
{
    uint8_t cmd;
    HAL_StatusTypeDef status;

    cmd = ADS1220_CMD_RREG | (reg << 2);

    ADS1220_CS_LOW();
    status = HAL_SPI_Transmit(hads->hspi, &cmd, 1, ADS1220_TIMEOUT);
    if (status == HAL_OK) {
        status = HAL_SPI_Receive(hads->hspi, value, 1, ADS1220_TIMEOUT);
    }
    ADS1220_CS_HIGH();

    return (status == HAL_OK) ? ADS1220_OK : ADS1220_ERROR;
}

/**
  * @brief  Check if data is ready
  * @param  hads: Pointer to ADS1220 handle
  * @retval 1 if ready, 0 if not ready
  */
uint8_t ADS1220_DataReady(ADS1220_Handle_t *hads)
{
    return (HAL_GPIO_ReadPin(hads->drdy_port, hads->drdy_pin) == GPIO_PIN_RESET) ? 1 : 0;
}

// /**
  // * @brief  Read 24-bit conversion data from ADS1220
  // * @param  hads: Pointer to ADS1220 handle
  // * @param  data: Pointer to store 24-bit signed result
  // * @retval Status
  // */
// ADS1220_Status_t ADS1220_ReadData(ADS1220_Handle_t *hads, int32_t *data)
// {
    // uint8_t buffer[3];
    // HAL_StatusTypeDef status;

    // /* Send START/SYNC command */
    // if (ADS1220_SendCommand(hads, ADS1220_CMD_START) != ADS1220_OK) {
        // return ADS1220_ERROR;
    // }

    // /* Wait for DRDY to go low (max ~50ms for 20 SPS) */
    // uint32_t timeout = HAL_GetTick() + 100;
    // while (ADS1220_DataReady(hads) == 0) {
        // if (HAL_GetTick() > timeout) {
            // return ADS1220_TIMEOUT;
        // }
    // }

    // /* Read 3 bytes of data */
    // uint8_t cmd = ADS1220_CMD_RDATA;
    // ADS1220_CS_LOW();
    // status = HAL_SPI_Transmit(hads->hspi, &cmd, 1, ADS1220_TIMEOUT);
    // if (status == HAL_OK) {
        // status = HAL_SPI_Receive(hads->hspi, buffer, 3, ADS1220_TIMEOUT);
    // }
    // ADS1220_CS_HIGH();

    // if (status != HAL_OK) {
        // return ADS1220_ERROR;
    // }

    // /* Convert 24-bit signed to 32-bit signed */
    // int32_t raw = ((int32_t)buffer[0] << 16) | ((int32_t)buffer[1] << 8) | buffer[2];
    
    // /* Sign extend from 24-bit to 32-bit */
    // if (raw & 0x800000) {
        // raw |= 0xFF000000;
    // }

    // *data = raw;
    // return ADS1220_OK;
// }

/**
  * @brief  Read 24-bit conversion data from ADS1220
  * @param  hads: Pointer to ADS1220 handle
  * @param  data: Pointer to store 24-bit signed result
  * @retval Status
  */
ADS1220_Status_t ADS1220_ReadData(ADS1220_Handle_t *hads, int32_t *data)
{
    uint8_t buffer[3];
    HAL_StatusTypeDef status;

    /* Wait a bit for conversion (50ms for 20 SPS) */
    HAL_Delay(50);

    /* Read 3 bytes of data directly */
    uint8_t cmd = ADS1220_CMD_RDATA;
    ADS1220_CS_LOW();
    status = HAL_SPI_Transmit(hads->hspi, &cmd, 1, ADS1220_TIMEOUT);
    if (status == HAL_OK) {
        status = HAL_SPI_Receive(hads->hspi, buffer, 3, ADS1220_TIMEOUT);
    }
    ADS1220_CS_HIGH();

    if (status != HAL_OK) {
        return ADS1220_ERROR;
    }

    /* Convert 24-bit signed to 32-bit signed */
    int32_t raw = ((int32_t)buffer[0] << 16) | ((int32_t)buffer[1] << 8) | buffer[2];
    
    /* Sign extend from 24-bit to 32-bit */
    if (raw & 0x800000) {
        raw |= 0xFF000000;
    }

    *data = raw;
    return ADS1220_OK;
}

/**
  * @brief  Convert ADC code to voltage
  * @param  hads: Pointer to ADS1220 handle
  * @param  code: 24-bit ADC code
  * @retval Voltage in volts
  */
float ADS1220_CodeToVoltage(ADS1220_Handle_t *hads, int32_t code)
{
    /* Full scale range = ±Vref/Gain */
    float lsb = (2.0f * hads->vref) / ((float)hads->gain * 8388608.0f);
    return (float)code * lsb;
}