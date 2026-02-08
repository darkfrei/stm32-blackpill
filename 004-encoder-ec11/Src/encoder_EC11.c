#include "encoder_EC11.h"

#define EC11_TICKS_PER_STEP 4

void EC11_Init(EC11_Encoder_t *enc)
{
    enc->step = 0;
    enc->tick = 0;
    enc->dir  = EC11_DIR_NONE;

    enc->buttonState = 1;
    enc->buttonPressed = 0;

    enc->lastTimerValue = 0;
}

int32_t EC11_TimerDiff16(EC11_Encoder_t *enc, uint16_t currentValue)
{
    int32_t diff = (int32_t)currentValue - (int32_t)enc->lastTimerValue;

    if (diff > 32767) {
        diff -= 65536;
    } else if (diff < -32768) {
        diff += 65536;
    }

    enc->lastTimerValue = currentValue;
    return diff;
}

void EC11_ProcessTicks(EC11_Encoder_t *enc, int32_t diff)
{
    if (diff == 0) return;

    enc->dir = (diff > 0) ? EC11_DIR_CW : EC11_DIR_CCW;
    enc->tick += diff;

    while (enc->tick >= EC11_TICKS_PER_STEP) {
        enc->step++;
        enc->tick -= EC11_TICKS_PER_STEP;
    }

    while (enc->tick <= -EC11_TICKS_PER_STEP) {
        enc->step--;
        enc->tick += EC11_TICKS_PER_STEP;
    }
}

void EC11_ProcessButton(EC11_Encoder_t *enc,
                         uint8_t rawState,
                         uint32_t nowMs,
                         uint32_t debounceMs)
{
    static uint8_t lastRaw = 1;
    static uint32_t lastChange = 0;

    if (rawState != lastRaw) {
        lastRaw = rawState;
        lastChange = nowMs;
    }

    if ((nowMs - lastChange) >= debounceMs) {
        if (rawState != enc->buttonState) {
            enc->buttonState = rawState;
            if (rawState == 0) {
                enc->buttonPressed = 1;
            }
        }
    }
}

void EC11_Reset(EC11_Encoder_t *enc)
{
    enc->step = 0;
    enc->tick = 0;
    enc->dir  = EC11_DIR_NONE;
    enc->buttonPressed = 0;
}
