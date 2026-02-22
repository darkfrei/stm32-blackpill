#include "stroboscope.h"
#include "main.h"

Strobe_t strobe = {30, 2, 75, 0};

void Strobe_Init(void) {
    /* TIM1 + TIM3 setup already done in CubeMX */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
}

void Strobe_Start(void) {
    strobe.running = 1;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (strobe.bright * (TIM1_PWM_PERIOD+1))/100);
    HAL_TIM_OC_Start_IT(&htim3, TIM_CHANNEL_1);
    HAL_TIM_Base_Start_IT(&htim3);
}

void Strobe_Stop(void) {
    strobe.running = 0;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
    HAL_TIM_OC_Stop_IT(&htim3, TIM_CHANNEL_1);
    HAL_TIM_Base_Stop_IT(&htim3);
}

void Strobe_SetFreq(uint32_t hz) {
    if(hz < STROBE_FREQ_MIN) hz = STROBE_FREQ_MIN;
    if(hz > STROBE_FREQ_MAX) hz = STROBE_FREQ_MAX;
    strobe.freq = hz;
    __HAL_TIM_SET_AUTORELOAD(&htim3, (TIM3_TICK_FREQ / strobe.freq) - 1);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ((TIM3->ARR+1)*strobe.duty)/100);
}

void Strobe_SetDuty(uint8_t duty) {
    if(duty < STROBE_DUTY_MIN) duty = STROBE_DUTY_MIN;
    if(duty > STROBE_DUTY_MAX) duty = STROBE_DUTY_MAX;
    strobe.duty = duty;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ((TIM3->ARR+1)*strobe.duty)/100);
}

void Strobe_SetBright(uint8_t bright) {
    if(bright < STROBE_BRIGHT_MIN) bright = STROBE_BRIGHT_MIN;
    if(bright > STROBE_BRIGHT_MAX) bright = STROBE_BRIGHT_MAX;
    strobe.bright = bright;
    if(strobe.running) {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (strobe.bright*(TIM1_PWM_PERIOD+1))/100);
    }
}

void Strobe_Update(void) {
    if(strobe.running) {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (strobe.bright*(TIM1_PWM_PERIOD+1))/100);
    } else {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
    }
}