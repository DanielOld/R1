#ifndef LCD_H__
#define LCD_H__

#define LCD_WITH 96
#define LCD_HIGHT 96
#define LCD_COLOR_DEEP 2
#define LCD_BUFFER_SIZE (LCD_WITH*LCD_HIGHT*LCD_COLOR_DEEP)

void lcd_init(void);
void lcd_sleep(void);
void lcd_wakeup(void);
uint8_t* lcd_get_buffer_address(void);
void lcd_display(uint8_t startx,
                 uint8_t starty,
                 uint8_t endx,
                 uint8_t endy);

#endif /* LCD_H__ */
