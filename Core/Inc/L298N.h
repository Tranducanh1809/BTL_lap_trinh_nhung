#ifndef INC_L298N_H_
#define INC_L298N_H_

#include "main.h"

/* TIM3 CH3 */
extern TIM_HandleTypeDef htim3;

#define L298N_IN1_PORT      GPIOA
#define L298N_IN1_PIN       GPIO_PIN_0

#define L298N_IN2_PORT      GPIOA
#define L298N_IN2_PIN       GPIO_PIN_1

#define L298N_PWM_TIMER     (&htim3)
#define L298N_PWM_CHANNEL   TIM_CHANNEL_3

#define DOOR_CLOSED_PORT    GPIOC
#define DOOR_CLOSED_PIN     GPIO_PIN_14

#define DOOR_OPEN_PORT      GPIOC
#define DOOR_OPEN_PIN       GPIO_PIN_15

typedef enum
{
    DOOR_CLOSED = 0,
    DOOR_OPENING,
    DOOR_OPEN,
    DOOR_CLOSING,
    DOOR_REOPENING

} DoorState_t;

typedef struct
{
    GPIO_TypeDef *IN1_Port;
    uint16_t IN1_Pin;

    GPIO_TypeDef *IN2_Port;
    uint16_t IN2_Pin;

} L298N_t;


void L298N_Init(L298N_t *motor);

void L298N_Forward(L298N_t *motor, uint16_t speed); // mo cua

void L298N_Reverse(L298N_t *motor, uint16_t speed); // dong cua 

void L298N_Stop(L298N_t *motor);

uint8_t Door_IsClosed(void);
uint8_t Door_IsOpened(void);


#endif
