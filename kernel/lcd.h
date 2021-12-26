#ifndef __KERNEL_LCD_H__
#define __KERNEL_LCD_H__

/**
 * @file kernel/lcd.h
 *
 * LCD output code.
 */

int  lcd_init(void);
void lcd_putc(unsigned, char, int, int);
void lcd_copy(unsigned, unsigned, size_t);
void lcd_fill(unsigned, size_t, int, int);
void lcd_move_cursor(unsigned);

#endif  // !__KERNEL_LCD_H__
