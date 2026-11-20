
#ifndef BUZZER_H
#define BUZZER_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

void Buzzer_Init(void);

/* freq: tan so (Hz)
   volume: do keu tu 0 den 100 (% duty PWM)
   duration_ms: thoi gian keu (ms) */
void Buzzer_Beep(uint32_t freq,
                 uint8_t volume,
                 uint32_t duration_ms);

void Buzzer_Stop(void);
void Buzzer_Update(void);

#endif
