
#ifndef __DFPLAYER_H
#define __DFPLAYER_H

#include "main.h"

void DFPlayer_Init(void);
void DFPlayer_SetVolume(uint8_t volume);
void DFPlayer_Play(uint16_t track);
void DFPlayer_Stop(void);

#endif
