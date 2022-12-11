// See PrimeCell Color LCD Controller (PL111) Technical Reference Manual.

#include "pl111.h"

// LCD registers, shifted right by 2 bits for use as uint32_t[] 
#define LCD_TIMING0       (0x000 / 4)     // Horizontal Axis Panel Control
#define LCD_TIMING1       (0x004 / 4)     // Vertical Axis Panel Control
#define LCD_TIMING2       (0x008 / 4)     // Clock and Signal Polarity Control
#define LCD_UPBASE        (0x010 / 4)     // Upper Panel Frame Base Address
#define LCD_CONTROL       (0x018 / 4)     // LCD Control
#define   LCD_EN            (1U << 0)     //   CLCDC Enable
#define   LCD_BPP16         (6U << 1)     //   16 bits per pixel
#define   LCD_PWR           (1U << 11)    //   LCD Power Enable

// Magic timing values for different display resolutions
// https://developer.arm.com/documentation/dui0440/b/programmer-s-reference/color-lcd-controller--clcdc/display-resolutions-and-display-memory-organization
static int
timing_values[][3] = {
  [PL111_RES_QVGA_PORTRAIT]   { 0xC7A7BF38, 0x595B613F, 0x04eF1800 },
  [PL111_RES_QVGA_LANDSCAPE]  { 0x9F7FBF4C, 0x818360EF, 0x053F1800 },
  [PL111_RES_QCIF_PORTRAIT]   { 0xE7C7BF28, 0x8B8D60DB, 0x04AF1800 },
  [PL111_RES_VGA]             { 0x3F1F3F9C, 0x090B61DF, 0x067F1800 },
  [PL111_RES_SVGA]            { 0x1313A4C4, 0x0505F657, 0x071F1800 },
  [PL111_RES_EPSON]           { 0x02010228, 0x010004DB, 0x04AF3800 },
  [PL111_RES_SANYO]           { 0x0505054C, 0x050514EF, 0x053F1800 },
  [PL111_RES_XGA]             { 0x972F67FC, 0x17030EFF, 0x07FF3800 },
};

/**
 * Initialize the LCD driver.
 *
 * @param pl111 Pointer to the driver instance.
 * @param base Memory base address.
 * @param frame_buf Frame buffer physical base address.
 * @param res Requried display resolution.
 */
int
pl111_init(struct Pl111 *pl111, void *base, physaddr_t frame_buf,
           enum Pl111Resolution res)
{
  pl111->base = (volatile uint32_t *) base;

  // Define display timings
  pl111->base[LCD_TIMING0] = timing_values[res][0];
  pl111->base[LCD_TIMING1] = timing_values[res][1];
  pl111->base[LCD_TIMING2] = timing_values[res][2];

  // Setup frame buffer
  pl111->base[LCD_UPBASE] = frame_buf;

  // Enable LCD, 16 bpp.
  pl111->base[LCD_CONTROL] = LCD_EN | LCD_BPP16 | LCD_PWR;

  return 0;
}
