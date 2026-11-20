#include "l298n.h"

void L298N_Init(L298N_t *motor)
{
    motor->IN1_Port = L298N_IN1_PORT;
    motor->IN1_Pin  = L298N_IN1_PIN;

    motor->IN2_Port = L298N_IN2_PORT;
    motor->IN2_Pin  = L298N_IN2_PIN;

    HAL_GPIO_WritePin(
        motor->IN1_Port,
        motor->IN1_Pin,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        motor->IN2_Port,
        motor->IN2_Pin,
        GPIO_PIN_RESET
    );

    HAL_TIM_PWM_Start(
        L298N_PWM_TIMER,
        L298N_PWM_CHANNEL
    );

    __HAL_TIM_SET_COMPARE(
        L298N_PWM_TIMER,
        L298N_PWM_CHANNEL,
        0
    );
}


void L298N_Forward(L298N_t *motor, uint16_t speed)
{
    if (speed > 999)
        speed = 999;

    HAL_GPIO_WritePin(
        motor->IN1_Port,
        motor->IN1_Pin,
        GPIO_PIN_SET
    );

    HAL_GPIO_WritePin(
        motor->IN2_Port,
        motor->IN2_Pin,
        GPIO_PIN_RESET
    );

    __HAL_TIM_SET_COMPARE(
        L298N_PWM_TIMER,
        L298N_PWM_CHANNEL,
        speed
    );
}
void L298N_Reverse(L298N_t *motor, uint16_t speed)
{
    if (speed > 999)
        speed = 999;

    HAL_GPIO_WritePin(
        motor->IN1_Port,
        motor->IN1_Pin,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        motor->IN2_Port,
        motor->IN2_Pin,
        GPIO_PIN_SET
    );

    __HAL_TIM_SET_COMPARE(
        L298N_PWM_TIMER,
        L298N_PWM_CHANNEL,
        speed
    );
}


void L298N_Stop(L298N_t *motor)
{ 
	  HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_3);
    HAL_GPIO_WritePin(
        motor->IN1_Port,
        motor->IN1_Pin,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        motor->IN2_Port,
        motor->IN2_Pin,
        GPIO_PIN_RESET
    );

    __HAL_TIM_SET_COMPARE(
        L298N_PWM_TIMER,
        L298N_PWM_CHANNEL,
        0
    );
}
uint8_t Door_IsClosed(void)
{
    if (HAL_GPIO_ReadPin(DOOR_CLOSED_PORT,
                         DOOR_CLOSED_PIN) == GPIO_PIN_RESET)
    {
        return 1;
    }

    return 0;
}

uint8_t Door_IsOpened(void)
{
    if (HAL_GPIO_ReadPin(DOOR_OPEN_PORT,
                         DOOR_OPEN_PIN) == GPIO_PIN_RESET)
    {
        return 1;
    }

    return 0;
}

