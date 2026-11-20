#ifndef __RC522_H
#define __RC522_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "main.h"
// PB 11 -> rst
// PB 12 -> SDA
// PB 13 -> SCL
// PB 14 -> MISO
// PB 15 -> MOSI
// Ð?nh nghia chân k?t n?i (C?n kh?p v?i c?u hình trong CubeMX/Schematic)
#define RC522_CS_GPIO_Port  GPIOB
#define RC522_CS_Pin        GPIO_PIN_12
#define RC522_RST_GPIO_Port GPIOB
#define RC522_RST_Pin       GPIO_PIN_2

// Tr?ng thái tr? v?
#define MI_OK           0
#define MI_NOTAGERR     1
#define MI_ERR          2

// Các thanh ghi MFRC522
#define PCD_IDLE        0x00
#define PCD_AUTHENT     0x0E
#define PCD_RECEIVE     0x08
#define PCD_TRANSMIT    0x04
#define PCD_TRANSCEIVE  0x0C
#define PCD_RESETPHASE  0x0F
#define PCD_CALCCRC     0x03

// L?nh cho th? Mifare
#define PICC_REQIDL     0x26
#define PICC_REQALL     0x52
#define PICC_ANTICOLL   0x93
#define PICC_SElECTTAG  0x93
#define PICC_AUTHENT1A  0x60
#define PICC_AUTHENT1B  0x61
#define PICC_READ       0x30
#define PICC_WRITE      0xA0
#define PICC_DECREMENT  0xC0
#define PICC_INCREMENT  0xC1
#define PICC_RESTORE    0xC2
#define PICC_TRANSFER   0xB0
#define PICC_HALT       0x50

// Thanh ghi MFRC522 (Page 0: Command and Status)
#define MFRC522_REG_COMMAND       0x01
#define MFRC522_REG_COMM_IEN      0x02
#define MFRC522_REG_DIV1_IEN      0x03
#define MFRC522_REG_COMM_IRQ      0x04
#define MFRC522_REG_DIV_IRQ       0x05
#define MFRC522_REG_ERROR         0x06
#define MFRC522_REG_STATUS1       0x07
#define MFRC522_REG_STATUS2       0x08
#define MFRC522_REG_FIFO_DATA     0x09
#define MFRC522_REG_FIFO_LEVEL    0x0A
#define MFRC522_REG_WATER_LEVEL   0x0B
#define MFRC522_REG_CONTROL       0x0C
#define MFRC522_REG_BIT_FRAMING   0x0D
#define MFRC522_REG_COLL          0x0E

// Thanh ghi MFRC522 (Page 1: Command)
#define MFRC522_REG_MODE          0x11
#define MFRC522_REG_TX_MODE       0x12
#define MFRC522_REG_RX_MODE       0x13
#define MFRC522_REG_TX_CONTROL    0x14
#define MFRC522_REG_TX_AUTO       0x15

// Thanh ghi MFRC522 (Page 2: Configuration)
#define MFRC522_REG_CRC_RESULT_M  0x21
#define MFRC522_REG_CRC_RESULT_L  0x22
#define MFRC522_REG_RF_CFG        0x26
#define MFRC522_REG_T_MODE        0x2A
#define MFRC522_REG_T_PRESCALER   0x2B
#define MFRC522_REG_T_RELOAD_H    0x2C
#define MFRC522_REG_T_RELOAD_L    0x2D

// Nguyên m?u hàm
void MFRC522_Init(void);
uint8_t MFRC522_Check(uint8_t *id);
uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *TagType);
uint8_t MFRC522_Anticoll(uint8_t *serNum);
uint8_t MFRC522_SelectTag(uint8_t *serNum);
void MFRC522_Halt(void);
void MFRC522_WriteRegister(uint8_t addr, uint8_t val);
uint8_t MFRC522_ReadRegister(uint8_t addr);
uint8_t MFRC522_CheckUID(uint8_t *id) ;   // kiem dung UID 63 8B CA 06 tra ve 1
uint8_t MFRC522_ReadUID(uint8_t *id);
#ifdef __cplusplus
}
#endif

#endif
