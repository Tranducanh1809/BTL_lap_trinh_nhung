
#include "buzzer.h"


extern TIM_HandleTypeDef htim1;

#define BUZZER_TIMER       htim1
#define BUZZER_CHANNEL     TIM_CHANNEL_1

/* Timer counter = 1 MHz */
#define BUZZER_TIMER_CLOCK 72000000UL
#define BUZZER_COUNTER_HZ  1000000UL

static uint32_t buzzer_end_time = 0;
static uint8_t buzzer_running = 0;

void Buzzer_Init(void)
{
    HAL_TIM_PWM_Start(&BUZZER_TIMER, BUZZER_CHANNEL);

    Buzzer_Stop();
}

void Buzzer_Beep(uint32_t freq,
                 uint8_t volume,
                 uint32_t duration_ms)
{
    uint32_t arr;
    uint32_t ccr;

    if (freq == 0 || duration_ms == 0 || volume == 0)
    {
        Buzzer_Stop();
        return;
    }

    if (volume > 100)
        volume = 100;
    __HAL_TIM_SET_PRESCALER(
        &BUZZER_TIMER,
        (BUZZER_TIMER_CLOCK / BUZZER_COUNTER_HZ) - 1
    );

    arr = BUZZER_COUNTER_HZ / freq - 1;
    ccr = ((arr + 1) * volume) / 100;

    __HAL_TIM_SET_AUTORELOAD(&BUZZER_TIMER, arr);
    __HAL_TIM_SET_COMPARE(
        &BUZZER_TIMER,
        BUZZER_CHANNEL,
        ccr
    );

    __HAL_TIM_SET_COUNTER(&BUZZER_TIMER, 0);
    BUZZER_TIMER.Instance->EGR = TIM_EGR_UG;

    buzzer_end_time = HAL_GetTick() + duration_ms;
    buzzer_running = 1;
}

void Buzzer_Stop(void)
{
    __HAL_TIM_SET_COMPARE(
        &BUZZER_TIMER,
        BUZZER_CHANNEL,
        0
    );

    buzzer_running = 0;
}

void Buzzer_Update(void)
{
    if (buzzer_running &&
        (int32_t)(HAL_GetTick() - buzzer_end_time) >= 0)
    {
        Buzzer_Stop();
    }
}
