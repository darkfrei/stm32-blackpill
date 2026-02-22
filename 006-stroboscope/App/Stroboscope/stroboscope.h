#ifndef __STROBOSCOPE_H
#define __STROBOSCOPE_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef struct {
    uint32_t freq;      /* Hz */
    uint8_t  duty;      /* % */
    uint8_t  bright;    /* % */
    uint8_t  running;   /* 0 = off, 1 = on */
} Strobe_t;

void Strobe_Init(void);
void Strobe_Start(void);
void Strobe_Stop(void);
void Strobe_SetFreq(uint32_t hz);
void Strobe_SetDuty(uint8_t duty);
void Strobe_SetBright(uint8_t bright);
void Strobe_Update(void); /* call from TIM3 IRQ */

extern Strobe_t strobe;

#endif