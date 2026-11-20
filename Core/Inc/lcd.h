

#include "stm32f1xx_hal.h"
typedef enum
{
    LCD_NORMAL = 0,
    LCD_CARD_OK,
    LCD_CARD_WRONG,
    LCD_RFID_LOCKED,
    LCD_OBSTACLE,
    LCD_CARD_ENROLLING,
    LCD_CARD_SAVED,
    LCD_CARD_DUPLICATE
} LCD_Message_t;
void lcd_init (void);   // initialize lcd

void lcd_send_cmd (char cmd);  // send command to the lcd

void lcd_send_data (char data);  // send data to the lcd

void lcd_send_string (char *str);  // send string to the lcd

void lcd_clear_display (void);	//clear display lcd

void lcd_goto_XY (int row, int col); //set proper location on screen
void LCD_ClearLine(uint8_t row);
void LCD_Show(char *line1, char *line2);
void LCD_ShowIfChanged(const char *line1, const char *line2);
void LCD_ShowIfChanged(const char *line1, const char *line2);
