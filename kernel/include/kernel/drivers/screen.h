#ifndef __KERNEL_INCLUDE_KERNEL_DRIVERS_SCREEN_H__
#define __KERNEL_INCLUDE_KERNEL_DRIVERS_SCREEN_H__

#define SCREEN_ESC_MAX    16   // The maximum number of esc parameters
#define SCREEN_COLS       80
#define SCREEN_ROWS       30

enum ParserState {
  PARSER_NORMAL,
  PARSER_ESC,
  PARSER_CSI,
};

struct Display;

struct Screen {
  int                 fg_color;                     // Current foreground color
  int                 bg_color;                     // Current background color
  enum ParserState    state;          
  unsigned            esc_params[SCREEN_ESC_MAX];  // The esc sequence parameters
  int                 esc_cur_param;                // Index of the current esc parameter
  int                 esc_question;
  struct {
    unsigned ch : 8;
    unsigned fg : 4;
    unsigned bg : 4;
  }                   buf[SCREEN_COLS * SCREEN_ROWS];
  unsigned            cols;
  unsigned            rows;
  unsigned            pos;

  struct Display     *display;
};

// ANSI color codes
#define COLOR_MASK            7
#define COLOR_BLACK           0
#define COLOR_RED             1
#define COLOR_GREEN           2
#define COLOR_YELLOW          3
#define COLOR_BLUE            4
#define COLOR_MAGENTA         5
#define COLOR_CYAN            6
#define COLOR_WHITE           7
#define COLOR_BRIGHT          (COLOR_MASK + 1)
#define COLOR_GRAY            (COLOR_BRIGHT + COLOR_BLACK)
#define COLOR_BRIGHT_RED      (COLOR_BRIGHT + COLOR_RED)
#define COLOR_BRIGHT_GREEN    (COLOR_BRIGHT + COLOR_GREEN)
#define COLOR_BRIGHT_YELLOW   (COLOR_BRIGHT + COLOR_YELLOW)
#define COLOR_BRIGHT_BLUE     (COLOR_BRIGHT + COLOR_BLUE)
#define COLOR_BRIGHT_MAGENTA  (COLOR_BRIGHT + COLOR_MAGENTA)
#define COLOR_BRIGHT_CYAN     (COLOR_BRIGHT + COLOR_CYAN)
#define COLOR_BRIGHT_WHITE    (COLOR_BRIGHT + COLOR_WHITE)

void screen_init(struct Screen *, struct Display *);
void screen_out_char(struct Screen *, char);
void screen_flush(struct Screen *);
void screen_backspace(struct Screen *);

#endif  // !__KERNEL_INCLUDE_KERNEL_DRIVERS_SCREEN_H__
