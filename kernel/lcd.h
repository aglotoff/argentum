#ifndef KERNEL_LCD_H
#define KERNEL_LCD_H

/**
 * @file kernel/lcd.h
 *
 * LCD output code.
 *
 * For more info, see PrimeCell Color LCD Controller (PL111) Technical Reference
 * Manual.
 */

#include <stdint.h>
 
#define LCD_BASE        0x10020000      ///< LCD base memory address

// LCD registers, shifted right by 2 bits for use as uint32_t[] 
#define LCD_TIMING0     (0x000 >> 2)    ///< Horizontal Axis Panel Control
#define LCD_TIMING1     (0x004 >> 2)    ///< Vertical Axis Panel Control
#define LCD_TIMING2     (0x008 >> 2)    ///< Clock and Signal Polarity Control
#define LCD_UPBASE      (0x010 >> 2)    ///< Upper Panel Frame Base Address
#define LCD_CONTROL     (0x018 >> 2)    ///< LCD Control
  #define LCD_EN        (1 << 0)        ///< CLCDC Enable
  #define LCD_BPP16     (4 << 1)        ///< 16 bits per pixel
  #define LCD_PWR       (1 << 11)       ///< LCD Power Enable


// PC Screen Font format
// See https://www.win.tue.nl/~aeb/linux/kbd/font-formats-1.html

#define PSF_MAGIC  0x0436   ///< Magic number

struct PsfHeader {
  uint16_t  magic;          ///< Must be PSF_MAGIC
	uint8_t   mode;	          ///< PSF font mode
	uint8_t   charsize;	      ///< Character size
} __attribute__((__packed__));


/**
 * Initialize the LCD driver.
 */
void lcd_init(void);

/**
 * Output character to the display.
 * 
 * @param c The character to be printed.
 */ 
void lcd_putc(char c);

#endif  // !KERNEL_LCD_H
