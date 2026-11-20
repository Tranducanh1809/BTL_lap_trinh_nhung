
#include "lcd.h"
#include "cmsis_os.h"
#include "string.h"
extern I2C_HandleTypeDef hi2c1;  // change your handler here accordingly

#define SLAVE_ADDRESS_LCD 0x4E // change this according to ur setup

void lcd_send_cmd (char cmd)
{
  char data_u, data_l;
	uint8_t data_t[4];
	data_u = (cmd&0xf0);
	data_l = ((cmd<<4)&0xf0);
	data_t[0] = data_u|0x0C;  //en=1, rs=0
	data_t[1] = data_u|0x08;  //en=0, rs=0
	data_t[2] = data_l|0x0C;  //en=1, rs=0
	data_t[3] = data_l|0x08;  //en=0, rs=0
	HAL_I2C_Master_Transmit (&hi2c1, SLAVE_ADDRESS_LCD,(uint8_t *) data_t, 4, 50);
}

void lcd_send_data (char data)
{
	char data_u, data_l;
	uint8_t data_t[4];
	data_u = (data&0xf0);
	data_l = ((data<<4)&0xf0);
	data_t[0] = data_u|0x0D;  //en=1, rs=0
	data_t[1] = data_u|0x09;  //en=0, rs=0
	data_t[2] = data_l|0x0D;  //en=1, rs=0
	data_t[3] = data_l|0x09;  //en=0, rs=0
	HAL_I2C_Master_Transmit (&hi2c1, SLAVE_ADDRESS_LCD,(uint8_t *) data_t, 4, 100);
}

void lcd_init (void)
{
	lcd_send_cmd (0x33); /* set 4-bits interface */
	lcd_send_cmd (0x32);
	HAL_Delay(50);
	lcd_send_cmd (0x28); /* start to set LCD function */
	HAL_Delay(50);
	lcd_send_cmd (0x01); /* clear display */
	HAL_Delay(50);
	lcd_send_cmd (0x06); /* set entry mode */
	HAL_Delay(50);
	lcd_send_cmd (0x0c); /* set display to on */	
	HAL_Delay(50);
	lcd_send_cmd (0x02); /* move cursor to home and set data address to 0 */
	HAL_Delay(50);
	lcd_send_cmd (0x80);
}

void lcd_send_string (char *str)
{
	while (*str) lcd_send_data (*str++);
}

void lcd_clear_display (void)
{
	lcd_send_cmd (0x01); //clear display
}


void lcd_goto_XY(int row, int col)
{
    uint8_t pos_Addr;

    if (row == 0)
    {
        pos_Addr = 0x80 + col;  // Dong 1
    }
    else
    {
        pos_Addr = 0xC0 + col;  // Dong 2
    }

    lcd_send_cmd(pos_Addr);
}
void LCD_ClearLine(uint8_t row)
{
    uint8_t i;

    if (row > 1)
    {
        return;
    }

    lcd_goto_XY(row, 0);

    for (i = 0; i < 16; i++)
    {
        lcd_send_data(' ');
    }

    lcd_goto_XY(row, 0);
}


void LCD_Show(char *line1, char *line2)
{
    char buf1[16];
    char buf2[16];
    uint8_t i;
    uint8_t j;

    /* Tao dong 1, mac dinh la dau cach */
    for (i = 0; i < 16; i++)
        buf1[i] = ' ';

    if (line1 != NULL)
    {
        for (i = 0; i < 16 && line1[i] != '\0'; i++)
            buf1[i] = line1[i];
    }

    /* Tao dong 2, mac dinh la dau cach */
    for (i = 0; i < 16; i++)
        buf2[i] = ' ';

    if (line2 != NULL)
    {
        for (i = 0; i < 16 && line2[i] != '\0'; i++)
            buf2[i] = line2[i];
    }

    /* Ghi dung 16 ky tu tren tung dong */
    lcd_goto_XY(0, 0);

    for (j = 0; j < 16; j++)
        lcd_send_data(buf1[j]);

    lcd_goto_XY(1, 0);

    for (j = 0; j < 16; j++)
        lcd_send_data(buf2[j]);
}
