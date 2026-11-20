#include "hcsr04.h"
// timer 2 
// PA10 input -> echo
// PA9 output -> trig
// dung mach chia ap 2 con tro 1 K
extern TIM_HandleTypeDef htim2;

#define TRIG_PIN GPIO_PIN_9
#define TRIG_PORT GPIOA
#define ECHO_PIN GPIO_PIN_10
#define ECHO_PORT GPIOA
uint32_t pMillis;
uint32_t Value1 = 0;
uint32_t Value2 = 0;
uint16_t Distance  = 0; 

void HCSR04_Init(void)
{
    HAL_TIM_Base_Start(&htim2);
  HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
}

uint8_t HCSR04_IsObjectDetected(void)
{
    uint32_t startTick;
    uint16_t startCount;
    uint16_t endCount;
    uint16_t duration;
    uint16_t distance;

    // Tao xung TRIG 10 us
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
    __HAL_TIM_SET_COUNTER(&htim2, 0);

    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);

    while (__HAL_TIM_GET_COUNTER(&htim2) < 10)
    {
    }

    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);

    // Cho Echo len muc cao, toi da 10 ms
    startTick = HAL_GetTick();

    while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_RESET)
    {
        if ((HAL_GetTick() - startTick) >= 10)
        {
            return 0; // Khong nhan duoc Echo
        }
    }

    startCount = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);

    // Cho Echo xuong muc thap, toi da 50 ms
    startTick = HAL_GetTick();

    while (HAL_GPIO_ReadPin(ECHO_PORT, ECHO_PIN) == GPIO_PIN_SET)
    {
        if ((HAL_GetTick() - startTick) >= 50)
        {
            return 0; // Echo bi keo dai hoac loi cam bien
        }
    }

    endCount = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);

    // Tinh thoi gian Echo, xu ly ca truong hop tran bo dem 16-bit
    duration = (uint16_t)(endCount - startCount);

    // TIM2 phai dem voi tan so 1 MHz
    distance = duration / 58;

    if (distance > 0 && distance < 6)
    {
        return 1; // Co vat can duoi 7 cm
    }

    return 0; // Khong phat hien vat can
}
