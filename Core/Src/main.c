/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd.h"
#include "L298N.h"
#include "hcsr04.h"
#include "rc522.h"
#include "stdio.h"
#include "string.h"
#include "buzzer.h"
#include "stdlib.h"
#include "dfplayer.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

osThreadId defaultTaskHandle;
osThreadId RFIDTaskHandle;
osThreadId DoorTaskHandle;
osThreadId ObstacleTaskHandle;
osThreadId LCDTaskHandle;
osThreadId UARTTaskHandle;
/* USER CODE BEGIN PV */
#define MAX_WRONG_CARD      3U
#define RFID_LOCK_TIME      60000U  
volatile uint16_t MOTOR_SPEED = 700;
volatile uint32_t DOOR_AUTO_CLOSE_TIME = 10000U;
volatile uint8_t AUTO_CLOSE_ENABLED = 1;
uint8_t closeWarningActive = 0;
uint32_t closeWarningStart = 0;
/* Yêu cầu điều khiển từ máy tính */
volatile uint8_t openRequest = 0;
volatile uint8_t closeRequest = 0;
#define UART_RX_BUF_SIZE  64
uint8_t  uartRxBuf[UART_RX_BUF_SIZE];
volatile uint16_t uartRxHead = 0;
volatile uint16_t uartRxTail = 0;
uint8_t  uartRxByte;
/* Sự kiện gửi về Python */
#define UART_EVENT_NONE       0
#define UART_EVENT_CARD_OK    1
#define UART_EVENT_CARD_WRONG 2
#define UART_EVENT_OBSTACLE   3
#define UART_EVENT_LOCKED     4
#define UART_EVENT_HISTORY_OPEN  5
#define UART_EVENT_HISTORY_CLOSE 6
#define MAX_CARDS 20
#define RFID_NAME_LEN 20U
#define RFID_FLASH_MAGIC 0x52464944UL 
#define RFID_FLASH_PAGE_ADDRESS 0x0800FC00UL

char lcdGreetingName[RFID_NAME_LEN] = "BAN";
typedef struct
{
    uint8_t uid[4];
    char name[RFID_NAME_LEN];
} RFID_Card_t;

typedef struct
{
    uint32_t magic;
    uint8_t count;
    RFID_Card_t cards[MAX_CARDS];
} RFID_Persist_t;

RFID_Card_t validCards[MAX_CARDS] =
{
    {{0x63, 0x8B, 0xCA, 0x06}, "Chua dat ten"}
};
volatile uint8_t uartEvent = UART_EVENT_NONE;
volatile uint8_t rfidLocked = 0;
uint32_t rfidLockStart = 0;
uint8_t CardID[5];
volatile DoorState_t doorState = DOOR_CLOSED;
volatile uint8_t wrongCardCount = 0;
volatile uint8_t obstacleDetected = 0;
L298N_t motor;
uint32_t doorTimer = 0;
volatile LCD_Message_t lcdMessage = LCD_NORMAL;
volatile uint32_t lcdMessageStart = 0;
uint8_t cardCount = 1;
volatile uint8_t addCardMode = 0;
volatile uint32_t doorClosedDisplayStart = 0;
volatile uint8_t doorJustClosed = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM1_Init(void);
static void MX_I2C2_Init(void);
void USART2_Init(void);
void StartDefaultTask(void const * argument);
void StartRFIDTask(void const * argument);
void StartDoorTask(void const * argument);
void StartObstacleTask(void const * argument);
void StartLCDTask(void const * argument);
void StartUARTTask(void const * argument);

/* USER CODE BEGIN PFP */
void LCD_SetMessage(LCD_Message_t msg);
void LCD_SetMessage(LCD_Message_t msg)
{
    lcdMessage = msg;
    lcdMessageStart = HAL_GetTick();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uint16_t next = (uartRxHead + 1) % UART_RX_BUF_SIZE;
        if (next != uartRxTail)
        {
            uartRxBuf[uartRxHead] = uartRxByte;
            uartRxHead = next;
        }
        HAL_UART_Receive_IT(&huart1, &uartRxByte, 1);
    }
    if (huart->Instance == USART1 &&
        __HAL_UART_GET_FLAG(huart, UART_FLAG_ORE))
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        HAL_UART_Receive_IT(&huart1, &uartRxByte, 1);
    }
}
static void UART_SendString(const char *str)
{
    HAL_UART_Transmit(&huart1,
                      (uint8_t *)str,
                      strlen(str),
                      100);
}
static const char *DoorState_ToString(DoorState_t state)
{
    switch (state)
    {
        case DOOR_CLOSED:   return "CLOSED";
        case DOOR_OPENING:  return "OPENING";
        case DOOR_OPEN:     return "OPEN";
        case DOOR_CLOSING:  return "CLOSING";
        case DOOR_REOPENING:return "REOPENING";
        default:            return "UNKNOWN";
    }
}


static void UART_SendDoorState(void)
{
    char msg[40];

    snprintf(msg, sizeof(msg),
             "STATE:%s\r\n",
             DoorState_ToString(doorState));

    UART_SendString(msg);
}
static int RFID_FindCard(uint8_t *uid)
{
    for (uint8_t i = 0; i < cardCount; i++)
    {
        if (memcmp(validCards[i].uid, uid, 4) == 0)
        {
            return i;
        }
    }

    return -1;
}

static int RFID_SaveCards(void)
{
    RFID_Persist_t image;
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t pageError = 0;
    HAL_StatusTypeDef status = HAL_OK;

    if (sizeof(RFID_Persist_t) % 2U != 0U)
        return 0;

    memset(&image, 0xFF, sizeof(image));
    image.magic = RFID_FLASH_MAGIC;
    image.count = cardCount;
    memcpy(image.cards, validCards, sizeof(validCards));

    HAL_FLASH_Unlock();
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = RFID_FLASH_PAGE_ADDRESS;
    erase.NbPages = 1;

    if (HAL_FLASHEx_Erase(&erase, &pageError) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 0;
    }

    const uint16_t *words = (const uint16_t *)&image;
    uint32_t address = RFID_FLASH_PAGE_ADDRESS;
    for (uint32_t i = 0; i < sizeof(image) / 2U; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              address, words[i]) != HAL_OK)
        {
            status = HAL_ERROR;
            break;
        }
        address += 2U;
    }
    HAL_FLASH_Lock();
    return (status == HAL_OK);
}

static void RFID_LoadCards(void)
{
    const RFID_Persist_t *stored =
        (const RFID_Persist_t *)RFID_FLASH_PAGE_ADDRESS;
    if (stored->magic != RFID_FLASH_MAGIC ||
        stored->count == 0U || stored->count > MAX_CARDS)
        return;

    memcpy(validCards, stored->cards, sizeof(validCards));
    cardCount = stored->count;
}

static int RFID_SetCardName(uint8_t *uid, const char *name)
{
    int index = RFID_FindCard(uid);
    size_t length = strlen(name);

    if (index < 0 || length == 0U || length >= RFID_NAME_LEN)
        return 0;

    /* Keep the wire format simple: a name cannot contain a colon/newline. */
    if (strchr(name, ':') != NULL || strchr(name, '\r') != NULL ||
        strchr(name, '\n') != NULL)
        return 0;

    memset(validCards[index].name, 0, RFID_NAME_LEN);
    memcpy(validCards[index].name, name, length);
    return RFID_SaveCards();
}

static int RFID_AddCard(uint8_t *uid)
{
    if (RFID_FindCard(uid) >= 0)
        return 0;  // Thẻ đã tồn tại

    if (cardCount >= MAX_CARDS)
        return -1; // Danh sách đầy

    memcpy(validCards[cardCount].uid, uid, 4);
    strncpy(validCards[cardCount].name, "Chua dat ten", RFID_NAME_LEN - 1U);
    validCards[cardCount].name[RFID_NAME_LEN - 1U] = '\0';
    cardCount++;

    if (!RFID_SaveCards())
    {
        cardCount--;
        return -2; 
    }
    return 1;      // Thêm thành công
}

static int RFID_DeleteCard(uint8_t *uid)
{
    int index = RFID_FindCard(uid);

    if (index < 0)
        return 0;

    // Không cho xóa thẻ cuối cùng
    if (cardCount <= 1)
        return -1;

    for (uint8_t i = index; i < cardCount - 1; i++)
    {
        validCards[i] = validCards[i + 1];
    }

    cardCount--;
    if (!RFID_SaveCards())
        return -2;
    return 1;
}

static void UART_SendUID(uint8_t *uid)
{
    char msg[32];

    snprintf(msg, sizeof(msg),
             "%02X%02X%02X%02X",
             uid[0], uid[1], uid[2], uid[3]);

    UART_SendString(msg);
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI2_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_TIM4_Init();
  MX_TIM1_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
	USART2_Init();
	DFPlayer_Init();
  lcd_init();
	MFRC522_Init();
	RFID_LoadCards();
	HCSR04_Init();
	L298N_Init(&motor);
	Buzzer_Init();
	DFPlayer_SetVolume(25);
	if (Door_IsClosed())
  {
    doorState = DOOR_CLOSED;
  }
  else if (Door_IsOpened())
  {
    doorState = DOOR_OPEN;
  }
  else
  {
    doorState = DOOR_CLOSED;
}
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of RFIDTask */
  osThreadDef(RFIDTask, StartRFIDTask, osPriorityNormal, 0, 256);
  RFIDTaskHandle = osThreadCreate(osThread(RFIDTask), NULL);

  /* definition and creation of DoorTask */
  osThreadDef(DoorTask, StartDoorTask, osPriorityHigh, 0, 256);
  DoorTaskHandle = osThreadCreate(osThread(DoorTask), NULL);

  /* definition and creation of ObstacleTask */
  osThreadDef(ObstacleTask, StartObstacleTask, osPriorityHigh, 0, 256);
  ObstacleTaskHandle = osThreadCreate(osThread(ObstacleTask), NULL);

  /* definition and creation of LCDTask */
  osThreadDef(LCDTask, StartLCDTask, osPriorityLow, 0, 256);
  LCDTaskHandle = osThreadCreate(osThread(LCDTask), NULL);

  /* definition and creation of UARTTask */
  osThreadDef(UARTTask, StartUARTTask, osPriorityNormal, 0, 256);
  UARTTaskHandle = osThreadCreate(osThread(UARTTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 71;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */

void USART2_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable clock GPIOA và USART2 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    /* PA2 = TX, PA3 = RX */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Cấu hình USART2 */
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 9600;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    HAL_UART_Init(&huart2);
}

static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PC14 PC15 */
  GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	
	GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB2 PB12 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PA10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    Buzzer_Update();
        osDelay(10);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartRFIDTask */
/**
* @brief Function implementing the RFIDTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartRFIDTask */
void StartRFIDTask(void const * argument)
{
  /* USER CODE BEGIN StartRFIDTask */
  /* Infinite loop */
	  uint8_t result;
    uint8_t prevCardPresent = 0;
    uint8_t CardID_Local[5];
    int addResult;
    int cardIndex;
    char msg[64];
  for(;;)
  {
        if (rfidLocked)
        {
            if ((HAL_GetTick() - rfidLockStart) >= RFID_LOCK_TIME)
            {
                rfidLocked = 0;
                wrongCardCount = 0;

                LCD_SetMessage(LCD_NORMAL);
                UART_SendString("RFID_UNLOCKED\r\n");
            }

            prevCardPresent = 0;
            osDelay(200);
            continue;
        }
        if (!addCardMode &&
    (doorState == DOOR_OPEN ||
     doorState == DOOR_OPENING ||
     doorState == DOOR_CLOSING ||
     doorState == DOOR_REOPENING))
        {
            prevCardPresent = 0;
            osDelay(200);
            continue;
        }
        result = MFRC522_ReadUID(CardID_Local);

        if (result == 1)
        {
            if (prevCardPresent == 0)
            {
                prevCardPresent = 1;
                if (addCardMode)
                {
                    addResult = RFID_AddCard(CardID_Local);

                    if (addResult == 1)
                {
                   snprintf(msg, sizeof(msg),
                   "CARD_ADDED:%02X%02X%02X%02X\r\n",
                   CardID_Local[0],
                   CardID_Local[1],
                   CardID_Local[2],
                   CardID_Local[3]);

                  UART_SendString(msg);
                  LCD_SetMessage(LCD_CARD_SAVED);
									DFPlayer_Play(10);
                }
                else if (addResult == 0)
                {
                  UART_SendString("ERR:CARD_EXISTS\r\n");

                  LCD_SetMessage(LCD_CARD_DUPLICATE);
									DFPlayer_Play(9);
                 }
                else if (addResult == -2)
                 {
                UART_SendString("ERR:FLASH_SAVE_FAILED\r\n");
                 }
                else
                {
               UART_SendString("ERR:CARD_LIST_FULL\r\n");
                }

              addCardMode = 0;
               UART_SendString("ADD_MODE:OFF\r\n");
                }
                else
                {
                    cardIndex = RFID_FindCard(CardID_Local);

                    if (cardIndex >= 0)
                    {
                    wrongCardCount = 0;
                    snprintf(lcdGreetingName,
                    sizeof(lcdGreetingName),
                        "%s",
                        validCards[cardIndex].name);
                        DFPlayer_Play(1);
                        LCD_SetMessage(LCD_CARD_OK);
                        uartEvent = UART_EVENT_CARD_OK;
                        Buzzer_Beep(2500, 50, 120);
                        if (doorState == DOOR_CLOSED)
                        {
                            openRequest = 1;
                        }

                        snprintf(msg, sizeof(msg),
                                 "CARD_OK:%02X%02X%02X%02X:%s\r\n",
                                 CardID_Local[0],
                                 CardID_Local[1],
                                 CardID_Local[2],
                                 CardID_Local[3],
                                 validCards[cardIndex].name);

                        UART_SendString(msg);
                    }
                    else
                    {
                        wrongCardCount++;

                        if (wrongCardCount >= MAX_WRONG_CARD)
                        {
                            rfidLocked = 1;
                            rfidLockStart = HAL_GetTick();
                            DFPlayer_Play(3);
                            LCD_SetMessage(LCD_RFID_LOCKED);
                            uartEvent = UART_EVENT_LOCKED;
                            Buzzer_Beep(500, 80, 1000);
                            UART_SendString(
                                "CARD_LOCKED:60\r\n");
                        }
                        else
                        {   
													  DFPlayer_Play(2);
                            LCD_SetMessage(LCD_CARD_WRONG);
                            uartEvent = UART_EVENT_CARD_WRONG;
                            Buzzer_Beep(1000, 60, 250);
													
                            snprintf(msg, sizeof(msg),
                                     "CARD_WRONG:%u\r\n",
                                     wrongCardCount);

                            UART_SendString(msg);
                        }
                    }
                }
            }
        }
        else
        {
            prevCardPresent = 0;
        }

        osDelay(200);
  }
  /* USER CODE END StartRFIDTask */
}

/* USER CODE BEGIN Header_StartDoorTask */
/**
* @brief Function implementing the DoorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDoorTask */
void StartDoorTask(void const * argument)
{
  /* USER CODE BEGIN StartDoorTask */
  /* Infinite loop */
	uint8_t buttonRaw;
  uint8_t buttonStable = 1;
  uint8_t buttonLastRaw = 1;
  uint32_t buttonDebounceTime = 0;
  for(;;)
  {  
   
  buttonRaw = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5);

  if (buttonRaw != buttonLastRaw)
  {
    buttonLastRaw = buttonRaw;
    buttonDebounceTime = HAL_GetTick();
  }

  if ((HAL_GetTick() - buttonDebounceTime) >= 30)
  {
    if (buttonStable != buttonRaw)
    {
        buttonStable = buttonRaw;
        if (buttonStable == GPIO_PIN_RESET)
        {
            if (doorState == DOOR_CLOSED)
            {
                openRequest = 1;
							  UART_SendString("BTN:OPEN\r\n");
            }
            else if (doorState == DOOR_OPEN)
            {
                closeRequest = 1;
							  UART_SendString("BTN:CLOSE\r\n");
            }
        }
    }
}		
    if (doorState == DOOR_CLOSED)
        {
            L298N_Stop(&motor);
            if (openRequest)
            {
                openRequest = 0;

                doorState = DOOR_OPENING;

                doorTimer = HAL_GetTick();

                L298N_Forward(&motor,
                              MOTOR_SPEED);
            }
        }

        else if (doorState == DOOR_OPENING)
        {
            if (Door_IsOpened())
            {
                L298N_Stop(&motor);
                doorState = DOOR_OPEN;
							  osDelay(1000);
							  DFPlayer_Play(4);
                doorTimer = HAL_GetTick();
            }
            else
            {
                L298N_Forward(&motor,
                              MOTOR_SPEED);
            }
        }
        else if (doorState == DOOR_OPEN)
        {
            L298N_Stop(&motor);
					  if (closeRequest)
            {
            closeRequest = 0;

            doorState = DOOR_CLOSING;
            doorTimer = HAL_GetTick();

        L298N_Reverse(&motor, MOTOR_SPEED);
            }
            
           else if (AUTO_CLOSE_ENABLED &&
         (HAL_GetTick() - doorTimer)
         >= (DOOR_AUTO_CLOSE_TIME - 2000))
{
    if (!closeWarningActive)
    {
        closeWarningActive = 1;
        closeWarningStart = HAL_GetTick();
        DFPlayer_Play(5);
    }

    if (HAL_GetTick() - closeWarningStart >= 2000)
    {
        closeWarningActive = 0;

        doorState = DOOR_CLOSING;
        doorTimer = HAL_GetTick();

        L298N_Reverse(&motor, MOTOR_SPEED);
    }
}
        }
        else if (doorState == DOOR_CLOSING)
        {
            if (obstacleDetected)
            {
                obstacleDetected = 0;
                L298N_Stop(&motor);
                doorState = DOOR_REOPENING;
                doorTimer = HAL_GetTick();
                LCD_SetMessage(LCD_OBSTACLE);

                osDelay(300);

                L298N_Reverse(&motor, MOTOR_SPEED);
            }
            else if (Door_IsClosed())
            {
                L298N_Stop(&motor);
                doorState = DOOR_CLOSED;
							  DFPlayer_Play(6);
							  doorClosedDisplayStart = HAL_GetTick();
                doorJustClosed = 1;
							  UART_SendString("HISTORY:CLOSE\r\n");
            }
            else
            {
                L298N_Reverse(&motor,
                              MOTOR_SPEED);
            }
        }

        else if (doorState == DOOR_REOPENING)
        {
            if (Door_IsOpened())
            {
                L298N_Stop(&motor);

                doorState = DOOR_OPEN;
                doorTimer = HAL_GetTick();
							  UART_SendString("HISTORY:OPEN\r\n");
            }
            else
            {
                L298N_Forward(&motor,
                              MOTOR_SPEED);
            }
        }


        osDelay(20);
    }
	
  /* USER CODE END StartDoorTask */
}

/* USER CODE BEGIN Header_StartObstacleTask */
/**
* @brief Function implementing the ObstacleTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartObstacleTask */
void StartObstacleTask(void const * argument)
{
  /* USER CODE BEGIN StartObstacleTask */
  /* Infinite loop */
    
    uint8_t detectCount = 0;

    for (;;)
    {
        if (doorState == DOOR_CLOSING)
        {
            if (HCSR04_IsObjectDetected() == 1)
            {
                if (detectCount < 3)
                {
                    detectCount++;
                }
                if (detectCount >= 3 && !obstacleDetected)
                {
                    obstacleDetected = 1;
                    uartEvent = UART_EVENT_OBSTACLE;
									  DFPlayer_Play(7);
                }
            }
            else
            {
                detectCount = 0;
            }
        }
        else
        {
            detectCount = 0;
        }

        osDelay(50);
    }


  /* USER CODE END StartObstacleTask */
}

/* USER CODE BEGIN Header_StartLCDTask */
/**
* @brief Function implementing the LCDTask thread.
* @param argument: Not used
* @retval None
*/
static void LCD_ShowIfChanged(const char *line1, const char *line2)
{
    static char oldLine1[17] = "";
    static char oldLine2[17] = "";

    if (strcmp(line1, oldLine1) != 0 ||
        strcmp(line2, oldLine2) != 0)
    {
        LCD_Show((char *)line1, (char *)line2);

        strncpy(oldLine1, line1, 16);
        oldLine1[16] = '\0';

        strncpy(oldLine2, line2, 16);
        oldLine2[16] = '\0';
    }
}
/* USER CODE END Header_StartLCDTask */
void StartLCDTask(void const * argument)
{
    uint32_t elapsed;
    char buf[17];
    for (;;)
    {
        elapsed = HAL_GetTick() - lcdMessageStart;
        if (rfidLocked)
        {
            uint32_t remain = RFID_LOCK_TIME -
                              (HAL_GetTick() - rfidLockStart);

            uint32_t sec = (remain + 999U) / 1000U;

            snprintf(buf, sizeof(buf),
                     "RETRY IN: %lu SEC",
                     (unsigned long)sec);

            LCD_ShowIfChanged("SYSTEM LOCKED!", buf);
        }
        else if (addCardMode)
        {
           LCD_ShowIfChanged("ADD CARD MODE",
             "SCAN YOUR CARD");
        }
        else if (lcdMessage == LCD_CARD_SAVED &&
         elapsed < 2000U)
        {
					Buzzer_Beep(2500, 50, 120);
          LCD_ShowIfChanged("CARD SAVED!",
             "ENROLL SUCCESS");
					
        }
        else if (lcdMessage == LCD_CARD_DUPLICATE &&
         elapsed < 2000U)
        {
				 Buzzer_Beep(500, 80, 1000);
         LCD_ShowIfChanged("CARD EXISTS!",
             "NOT ADDED");
				
        }
        else if (lcdMessage == LCD_CARD_OK && elapsed < 1500U)
        {
            LCD_ShowIfChanged("ACCESS GRANTED",
                     lcdGreetingName);
					
        }
        else if (lcdMessage == LCD_CARD_WRONG && elapsed < 1500U)
        {
            if (wrongCardCount == 1)
                LCD_ShowIfChanged("ACCESS DENIED",
                         "ATTEMPTS LEFT: 2");

            else if (wrongCardCount == 2)
                LCD_ShowIfChanged("ACCESS DENIED",
                         "ATTEMPTS LEFT: 1");
        }
        else if (lcdMessage == LCD_OBSTACLE && elapsed < 2000U)
        {
            LCD_ShowIfChanged("! OBSTACLE !",
                     "DOOR REOPENING");
        }
        else
        {
            lcdMessage = LCD_NORMAL;

            switch (doorState)
            {
                case DOOR_CLOSED:
                    LCD_ShowIfChanged("SMART DOOR",
                             "STATUS: CLOSED");
                    break;

                case DOOR_OPENING:
                    LCD_ShowIfChanged("SMART DOOR",
                             "OPENING...");
                    break;

                case DOOR_OPEN:
                    LCD_ShowIfChanged("SMART DOOR",
                             "STATUS: OPEN");
                    break;

                case DOOR_CLOSING:
                    LCD_ShowIfChanged("SMART DOOR",
                             "CLOSING...");
                    break;

                case DOOR_REOPENING:
                    LCD_ShowIfChanged("OBSTACLE DETECTED",
                             "DOOR REOPENING");
                    break;

                default:
                    LCD_ShowIfChanged("SMART DOOR",
                             "SYSTEM ERROR");
                    break;
            }
        }

        osDelay(200);
    }

  /* USER CODE END StartLCDTask */
}

/* USER CODE BEGIN Header_StartUARTTask */
/**
* @brief Function implementing the UARTTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUARTTask */
void StartUARTTask(void const * argument)
{
  /* USER CODE BEGIN StartUARTTask */
    char     rxBuffer[64];
    uint8_t  index = 0;
    uint32_t lastStateSend = 0;
    HAL_UART_Receive_IT(&huart1, &uartRxByte, 1);

    UART_SendString("SMART_DOOR:READY\r\n");
    UART_SendDoorState();

    for (;;)
    {
        while (uartRxTail != uartRxHead)
        {
            uint8_t ch = uartRxBuf[uartRxTail];
            uartRxTail = (uartRxTail + 1) % UART_RX_BUF_SIZE;

            if (ch == '\r')
            {
                continue;
            }
            else if (ch == '\n')
            {
                rxBuffer[index] = '\0';

                if (strcmp(rxBuffer, "OPEN") == 0)
                {
                    if (doorState == DOOR_CLOSED && !rfidLocked)
                    {
                        openRequest = 1;
                        UART_SendString("ACK:OPEN\r\n");
                    }
                    else
                    {
                        UART_SendString("ERR:OPEN_NOT_ALLOWED\r\n");
                    }
                }
                else if (strcmp(rxBuffer, "CLOSE") == 0)
                {
                    if (doorState == DOOR_OPEN)
                    {
                        closeRequest = 1;
                        UART_SendString("ACK:CLOSE\r\n");
                    }
                    else
                    {
                        UART_SendString("ERR:CLOSE_NOT_ALLOWED\r\n");
                    }
                }
								else if (strcmp(rxBuffer, "HISTORY:OPEN") == 0)
                {
                UART_SendString("ACK:HISTORY_OPEN\r\n");
                }
                 else if (strcmp(rxBuffer, "HISTORY:CLOSE") == 0)
                {
                UART_SendString("ACK:HISTORY_CLOSE\r\n");
                }
								
                    else if (strncmp(rxBuffer, "SET_CARD_NAME:", 14) == 0)
                    {
                        unsigned int b0, b1, b2, b3;
                        uint8_t uid[4];
                        const char *name = rxBuffer + 14;
                        if (strlen(name) >= 9U && name[8] == ':' &&
                            sscanf(name, "%2x%2x%2x%2x", &b0, &b1, &b2, &b3) == 4 &&
                            b0 <= 255U && b1 <= 255U && b2 <= 255U && b3 <= 255U)
                        {
                            uid[0] = (uint8_t)b0;
                            uid[1] = (uint8_t)b1;
                            uid[2] = (uint8_t)b2;
                            uid[3] = (uint8_t)b3;
                            name += 9; 
                            if (RFID_SetCardName(uid, name))
                                UART_SendString("ACK:CARD_NAME_SAVED\r\n");
                            else
                                UART_SendString("ERR:CARD_NAME_INVALID_OR_FLASH\r\n");
                        }
                        else
                        {
                            UART_SendString("ERR:SET_CARD_NAME_FORMAT\r\n");
                        }
                    }
                    else if (strcmp(rxBuffer, "ADD_CARD") == 0)
                    {
                        addCardMode = 1;
											  DFPlayer_Play(11);
											  LCD_SetMessage(LCD_CARD_ENROLLING);
                        UART_SendString("ADD_MODE:ON\r\n");
                    }
                    else if (strcmp(rxBuffer, "CANCEL_ADD") == 0)
                    {
                        addCardMode = 0;
											  DFPlayer_Play(8);
											  LCD_SetMessage(LCD_NORMAL);
                        UART_SendString("ADD_MODE:OFF\r\n");
                    }
                    else if (strcmp(rxBuffer, "LIST_CARDS") == 0)
                    {
                        char msg[64];

                        UART_SendString("CARD_LIST:BEGIN\r\n");

                        for (uint8_t i = 0; i < cardCount; i++)
                    {
                       snprintf(msg, sizeof(msg),
                       "CARD:%02X%02X%02X%02X:%s\r\n",
                       validCards[i].uid[0],
                       validCards[i].uid[1],
                       validCards[i].uid[2],
                       validCards[i].uid[3],
                       validCards[i].name);

                       UART_SendString(msg);
                    }

                       UART_SendString("CARD_LIST:END\r\n");
                    }
                    else if (strncmp(rxBuffer, "DELETE_CARD:", 12) == 0)
                    {
                       unsigned int b0, b1, b2, b3;
                       uint8_t uid[4];

                       if (strlen(rxBuffer) == 20 &&
                       sscanf(rxBuffer + 12, "%2x%2x%2x%2x",
                       &b0, &b1, &b2, &b3) == 4 &&
                       b0 <= 255 && b1 <= 255 &&
                       b2 <= 255 && b3 <= 255)
                    {
                       uid[0] = (uint8_t)b0;
                       uid[1] = (uint8_t)b1;
                       uid[2] = (uint8_t)b2;
                       uid[3] = (uint8_t)b3;

                    int deleteResult = RFID_DeleteCard(uid);

                     if (deleteResult == 1)
                      UART_SendString("CARD_DELETED\r\n");
                     else if (deleteResult == 0)
                      UART_SendString("ERR:CARD_NOT_FOUND\r\n");
                     else if (deleteResult == -2)
                      UART_SendString("ERR:FLASH_SAVE_FAILED\r\n");
                     else
                      UART_SendString("ERR:LAST_CARD\r\n");
                    }
                    else
                    {
                      UART_SendString("ERR:INVALID_UID\r\n");
                    }
                  }
										
                    else if (strncmp(rxBuffer, "MOTOR_SPEED:", 12) == 0)
                   {
                     uint32_t speed = atoi(rxBuffer + 12);

                    if (speed >= 10 && speed <= 100)
                      {
                      MOTOR_SPEED = speed * 10;

                      char msg[50];
                      snprintf(msg, sizeof(msg),
                      "ACK:MOTOR_SPEED:%lu\r\n",
                     (unsigned long)speed);

                      HAL_UART_Transmit(&huart1,
                         (uint8_t *)msg,
                         strlen(msg), 100);
                      }
                     }
                    
                   else if (strcmp(rxBuffer, "AUTO_CLOSE_ON") == 0)
                    {
                     AUTO_CLOSE_ENABLED = 1;
                     UART_SendString("ACK:AUTO_CLOSE_ON\r\n");
                    }
                     else if (strcmp(rxBuffer, "AUTO_CLOSE_OFF") == 0)
                    {
                     AUTO_CLOSE_ENABLED = 0;
                     UART_SendString("ACK:AUTO_CLOSE_OFF\r\n");
                    }

                    else if (strncmp(rxBuffer, "AUTO_CLOSE:", 11) == 0)
                    {
                      uint32_t seconds = atoi(rxBuffer + 11);

                    if (seconds >= 1 && seconds <= 120)
                    {
                      DOOR_AUTO_CLOSE_TIME = seconds * 1000U;

                      char msg[64];
                      snprintf(msg, sizeof(msg),
                      "ACK:AUTO_CLOSE:%lu\r\n",
                      (unsigned long)seconds);
                      HAL_UART_Transmit(&huart1, (uint8_t *)msg,
                          strlen(msg), 100);
                    }
                    else
                    {
                      HAL_UART_Transmit(&huart1,
                          (uint8_t *)"ERR:AUTO_CLOSE_RANGE\r\n",
                          22, 100);
                    }
                }

                else if (strcmp(rxBuffer, "STATUS") == 0)
                {
                    UART_SendDoorState();
                }
                else if (strcmp(rxBuffer, "RESET") == 0)
                {
                    rfidLocked     = 0;
                    wrongCardCount = 0;
                    LCD_SetMessage(LCD_NORMAL);
                    UART_SendString("ACK:RESET\r\n");
                }
                else if (index > 0)
                {
                    UART_SendString("ERR:UNKNOWN_COMMAND\r\n");
                }

                index = 0;
            }
            else
            {
                if (index < sizeof(rxBuffer) - 1)
                {
                    rxBuffer[index++] = (char)ch;
                }
                else
                {                   
                    index = 0;
                    UART_SendString("ERR:COMMAND_TOO_LONG\r\n");
                }
            }
        }
        if (uartEvent != UART_EVENT_NONE)
        {
            uint8_t event = uartEvent;
            uartEvent = UART_EVENT_NONE;

            switch (event)
            {
                case UART_EVENT_CARD_OK:
                    UART_SendString("CARD:OK\r\n");
                    break;

                case UART_EVENT_CARD_WRONG:
                    UART_SendString("CARD:WRONG\r\n");
                    break;

                case UART_EVENT_OBSTACLE:
                    UART_SendString("WARNING:OBSTACLE\r\n");
                    break;

                case UART_EVENT_LOCKED:
                    UART_SendString("WARNING:RFID_LOCKED\r\n");
                    break;
                case UART_EVENT_HISTORY_OPEN:
                    UART_SendString("HISTORY:OPEN\r\n");
                    break;

                case UART_EVENT_HISTORY_CLOSE:
                    UART_SendString("HISTORY:CLOSE\r\n");
                    break;
                default:
                    break;
            }
        }
        if (HAL_GetTick() - lastStateSend >= 500)
        {
            lastStateSend = HAL_GetTick();
            UART_SendDoorState();
        }
        osDelay(5);
    }
  /* USER CODE END StartUARTTask */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
