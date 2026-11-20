
#include "dfplayer.h"

extern UART_HandleTypeDef huart2;

/* Gui lenh UART cho DFPlayer */
static void DFPlayer_SendCommand(uint8_t cmd,
                                uint16_t parameter)
{
    uint8_t data[10];

    uint8_t paramH = parameter >> 8;
    uint8_t paramL = parameter & 0xFF;

    uint16_t checksum =
        0 - (0xFF + 0x06 + cmd + 0x00
             + paramH + paramL);

    data[0] = 0x7E;
    data[1] = 0xFF;
    data[2] = 0x06;
    data[3] = cmd;
    data[4] = 0x00;
    data[5] = paramH;
    data[6] = paramL;
    data[7] = checksum >> 8;
    data[8] = checksum & 0xFF;
    data[9] = 0xEF;

    HAL_UART_Transmit(&huart2,
                      data,
                      10,
                      100);
}

void DFPlayer_Init(void)
{
    HAL_Delay(2000);

    DFPlayer_SetVolume(20);
}

void DFPlayer_SetVolume(uint8_t volume)
{
    if (volume > 30)
        volume = 30;

    DFPlayer_SendCommand(0x06, volume);
}

void DFPlayer_Play(uint16_t track)
{
    DFPlayer_SendCommand(0x03, track);
}

void DFPlayer_Stop(void)
{
    DFPlayer_SendCommand(0x16, 0);
}
