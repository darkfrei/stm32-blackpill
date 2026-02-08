#ifndef ENCODER_EC11_H
#define ENCODER_EC11_H

#include <stdint.h>

typedef enum {
    EC11_DIR_NONE = 0,
    EC11_DIR_CW,
    EC11_DIR_CCW
} EC11_Dir_t;

typedef struct {
    int32_t step;
    int32_t tick;
    EC11_Dir_t dir;

    uint8_t buttonState;
    uint8_t buttonPressed;

    /* internal */
    uint16_t lastTimerValue;
} EC11_Encoder_t;

/* initialize encoder structure */
void EC11_Init(EC11_Encoder_t *enc);

/* calculate signed diff from 16-bit timer (with wraparound) */
int32_t EC11_TimerDiff16(EC11_Encoder_t *enc, uint16_t currentValue);

/* process rotation ticks */
void EC11_ProcessTicks(EC11_Encoder_t *enc, int32_t diff);

/* process button with debounce */
void EC11_ProcessButton(EC11_Encoder_t *enc,
                         uint8_t rawState,
                         uint32_t nowMs,
                         uint32_t debounceMs);

/* reset logical state */
void EC11_Reset(EC11_Encoder_t *enc);

#endif
