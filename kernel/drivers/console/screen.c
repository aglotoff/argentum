#include <string.h>

#include <kernel/drivers/screen.h>
#include <kernel/tty.h>
#include <kernel/drivers/display.h>
#include <kernel/drivers/kbd.h>
#include <kernel/types.h>

void
screen_init(struct Screen *screen, struct Display *display)
{
  unsigned i;

  screen->fg_color      = COLOR_WHITE;
  screen->bg_color      = COLOR_BLACK;
  screen->state         = PARSER_NORMAL;
  screen->esc_cur_param = -1;
  screen->cols          = SCREEN_COLS;
  screen->rows          = SCREEN_ROWS;
  screen->display       = display;
 
  for (i = 0; i < screen->cols * screen->rows; i++) {
    screen->buf[i].ch = ' ';
    screen->buf[i].fg = COLOR_WHITE;
    screen->buf[i].bg = COLOR_BLACK;
  }
}

int
screen_is_current(struct Screen *screen)
{
  return (tty_current != NULL) && (screen == tty_current->out.screen);
}

void
screen_flush(struct Screen *screen)
{
  if (screen_is_current(screen)) {
    display_flush(screen->display);
    display_update_cursor(screen->display);
  }
}

static void
screen_erase(struct Screen *screen, unsigned from, unsigned to)
{
  size_t i;

  for (i = from; i <= to; i++) {
    screen->buf[i].ch = ' ';
    screen->buf[i].fg = screen->fg_color;
    screen->buf[i].bg = screen->bg_color;
  }

  if (screen_is_current(screen))
    display_erase(screen->display, from, to);
}

static void
screen_set_char(struct Screen *screen, unsigned i, char c)
{
  screen->buf[i].ch = c;
  screen->buf[i].fg = screen->fg_color & 0xF;
  screen->buf[i].bg = screen->bg_color & 0xF;
}

#define DISPLAY_TAB_WIDTH  4

static void
screen_scroll_down(struct Screen *screen, unsigned n)
{
  unsigned i;

  if (screen_is_current(screen))
    display_flush(screen->display);

  memmove(&screen->buf[0], &screen->buf[screen->cols * n],
          sizeof(screen->buf[0]) * (screen->cols * (screen->rows - n)));

  for (i = screen->cols * (screen->rows - n); i < screen->cols * screen->rows; i++) {
    screen->buf[i].ch = ' ';
    screen->buf[i].fg = COLOR_WHITE;
    screen->buf[i].bg = COLOR_BLACK;
  }

  screen->pos -= n * screen->cols;

  if (screen_is_current(screen))
    display_scroll_down(screen->display, n);
}

static void
screen_insert_rows(struct Screen *screen, unsigned rows)
{
  unsigned max_rows, start_pos, end_pos, n, i;
  
  max_rows  = screen->rows - screen->pos / screen->cols;
  rows      = MIN(max_rows, rows);

  start_pos = screen->pos - (screen->pos % screen->cols);
  end_pos   = start_pos + rows * screen->cols;
  n         = (max_rows - rows) * screen->cols;

  if (screen_is_current(screen))
    display_flush(screen->display);

  for (i = 0; i < rows * screen->cols; i++) {
    screen->buf[i + start_pos].ch = ' ';
    screen->buf[i + start_pos].fg = COLOR_WHITE;
    screen->buf[i + start_pos].bg = COLOR_BLACK;
  }

  memmove(&screen->buf[end_pos], &screen->buf[start_pos],
          sizeof(screen->buf[0]) * n);
  
  if (screen_is_current(screen))
    display_flush(screen->display);
}

static int
screen_print_char(struct Screen *screen, char c)
{ 
  int ret = 0;

  switch (c) {
  case '\n':
    if (screen_is_current(screen))
      display_flush(screen->display);

    screen->pos += screen->cols;
    screen->pos -= screen->pos % screen->cols;

    if (screen_is_current(screen))
      display_update_cursor(screen->display);

    break;

  case '\r':
    if (screen_is_current(screen))
      display_flush(screen->display);

    screen->pos -= screen->pos % screen->cols;

    if (screen_is_current(screen))
      display_update_cursor(screen->display);

    break;

  case '\b':
    if (screen->pos > 0) {
      screen_flush(screen);

      screen->pos--;
      if (screen_is_current(screen))
        display_update_cursor(screen->display);
    }
    break;

  case '\t':
    do {
      screen_set_char(screen, screen->pos++, ' ');
      ret++;
    } while ((screen->pos % DISPLAY_TAB_WIDTH) != 0);
    break;

  case C('G'):
    // TODO: beep;
    break;

  default:
    if (c < ' ') {
      screen_set_char(screen, screen->pos++, '^');
      screen_set_char(screen, screen->pos++, '@' + c);
      ret += 2;
    } else {
      screen_set_char(screen, screen->pos++, c);
      ret++;
    }
    break;
  }

  if (screen->pos >= screen->cols * screen->rows)
    screen_scroll_down(screen, 1);

  return ret;
}

void
screen_dump(struct Screen *screen, unsigned c)
{
  const char sym[] = "0123456789ABCDEF";

  screen_print_char(screen, '~');
  screen_print_char(screen, '~');
  screen_print_char(screen, '~');

  screen_print_char(screen, sym[(c >> 28) & 0xF]);
  screen_print_char(screen, sym[(c >> 24) & 0xF]);
  screen_print_char(screen, sym[(c >> 20) & 0xF]);
  screen_print_char(screen, sym[(c >> 16) & 0xF]);
  screen_print_char(screen, sym[(c >> 12) & 0xF]);
  screen_print_char(screen, sym[(c >> 8) & 0xF]);
  screen_print_char(screen, sym[(c >> 4) & 0xF]);
  screen_print_char(screen, sym[(c >> 0) & 0xF]);

  screen_print_char(screen, '\n');
}

// Handle the escape sequence terminated by the final character c.
static void
screen_handle_esc(struct Screen *screen, char c)
{
  int i, tmp;
  unsigned n, m;

  screen_flush(screen);

  switch (c) {

  // Cursor Up
  case 'A':
    n = (screen->esc_cur_param == 0) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->pos / screen->cols);
    screen->pos -= n * screen->cols;
    break;
  
  // Cursor Down
  case 'B':
    n = (screen->esc_cur_param == 0) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->rows - screen->pos / screen->cols - 1);
    screen->pos += n * screen->cols;
    break;

  // Cursor Forward
  case 'C':
    n = (screen->esc_cur_param == 0) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->cols - screen->pos % screen->cols - 1);
    screen->pos += n;
    break;

  // Cursor Back
  case 'D':
    n = (screen->esc_cur_param == 0) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->pos % screen->cols);
    screen->pos -= n;
    break;

  // Cursor Horizontal Absolute
  case 'G':
    n = (screen->esc_cur_param < 1) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->cols);
    screen->pos -= screen->pos % screen->cols;
    screen->pos += n - 1;
    break;

  // Cursor Position
  case 'H':
    n = (screen->esc_cur_param < 1) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->rows);

    m = (screen->esc_cur_param < 2) ? 1 : screen->esc_params[1];
    m = MIN(m, screen->cols);

    screen->pos = (n - 1) * screen->cols + m - 1;
    break;

  // Erase in Display
  case 'J':
    n = (screen->esc_cur_param < 1) ? 0 : screen->esc_params[0];
    if (n == 0) {
      screen_erase(screen, screen->pos, SCREEN_COLS * SCREEN_ROWS - 1);
    } else if (n == 1) {
      screen_erase(screen, 0, screen->pos);
    } else if (n == 2) {
      screen_erase(screen, 0, SCREEN_COLS * SCREEN_ROWS - 1);
    }
    break;

  // Erase in Line
  case 'K':
    n = (screen->esc_cur_param < 1) ? 0 : screen->esc_params[0];
    if (n == 0) {
      screen_erase(screen, screen->pos, screen->pos - screen->pos % SCREEN_COLS + SCREEN_COLS - 1);
    } else if (n == 1) {
      screen_erase(screen, screen->pos - screen->pos % SCREEN_COLS, screen->pos);
    } else if (n == 2) {
      screen_erase(screen, screen->pos - screen->pos % SCREEN_COLS, screen->pos - screen->pos % SCREEN_COLS + SCREEN_COLS - 1);
    }
    break;

  // Insert Line
  case 'L':
    n = (screen->esc_cur_param < 1) ? 1 : screen->esc_params[0];
    screen_insert_rows(screen, n);
    break;

  // Cursor Vertical Position
  case 'd':
    n = (screen->esc_cur_param < 1) ? 1 : screen->esc_params[0];
    n = MIN(n, screen->rows);
    screen->pos = (n - 1) * screen->cols + screen->pos % screen->cols;
    break;

  case '@':
    n = (screen->esc_cur_param < 1) ? 1 : screen->esc_params[0];
    n = MIN(n, SCREEN_COLS - screen->pos % SCREEN_COLS);

    for (m = screen->pos - screen->pos % SCREEN_COLS + SCREEN_COLS - 1; m >= screen->pos; m--) {
      if (m > screen->pos) {
        screen->buf[m] = screen->buf[m - n];
      } else {
        screen->buf[m].ch = ' ';
        screen->buf[m].fg = screen->fg_color;
        screen->buf[m].bg = screen->bg_color;
      }

      if (screen_is_current(screen))
        display_draw_char_at(screen->display, m);
    }
    break;
 
  // Set Graphic Rendition
  case 'm':
    if (screen->esc_cur_param == 0) {
      // All attributes off
      screen->bg_color = COLOR_BLACK;
      screen->fg_color = COLOR_WHITE;
      break;
    }

    for (i = 0; (i < screen->esc_cur_param) && (i < SCREEN_ESC_MAX); i++) {
      switch (screen->esc_params[i]) {
      case 0:   // Reset all modes (styles and colors)
        screen->bg_color = COLOR_BLACK;
        screen->fg_color = COLOR_WHITE;
        break;
      case 1:   // Set bold mode
        screen->fg_color |= COLOR_BRIGHT;
        break;
      case 7:   // Set inverse/reverse mode
      case 27:
        tmp = screen->bg_color;
        screen->bg_color = screen->fg_color;
        screen->fg_color = tmp;
        break;
      case 22:  // Reset bold mode
        screen->fg_color &= ~COLOR_BRIGHT;
        break;
      case 39:
        // Default foreground color (white)
        screen->fg_color = (screen->fg_color & ~COLOR_MASK) | COLOR_WHITE;
        break;
      case 49:
        // Default background color (black)
        screen->bg_color = (screen->bg_color & ~COLOR_MASK) | COLOR_BLACK;
        break;
      default:
        if ((screen->esc_params[i] >= 30) && (screen->esc_params[i] <= 37)) {
          // Set foreground color
          screen->fg_color = (screen->fg_color & ~COLOR_MASK) | (screen->esc_params[i] - 30);
        } else if ((screen->esc_params[i] >= 40) && (screen->esc_params[i] <= 47)) {
          // Set background color
          screen->bg_color = (screen->bg_color & ~COLOR_MASK) | (screen->esc_params[i] - 40);
        }
      }
    }
    break;

  case 'b':
    for (n = 0; n < screen->esc_params[0]; n++)
      screen_print_char(screen, screen->buf[screen->pos - 1].ch);
    break;

  // TODO
  case 'n':
  case '%':
  case 'r':
  case 'h':
  case 'l':
    break;

  // TODO: handle other control sequences here

  default:
    screen_dump(screen, c);
    screen_dump(screen, screen->esc_params[0]);
    for (;;);

    break;
  }

  screen_flush(screen);
}

void
screen_out_char(struct Screen *screen, char c)
{
  int i;

  switch (screen->state) {
  case PARSER_NORMAL:
    if (c == '\x1b')
      screen->state = PARSER_ESC;
    else
      screen_print_char(screen, c);
    break;

	case PARSER_ESC:
		if (c == '[') {
			screen->state = PARSER_CSI;
      screen->esc_cur_param = -1;
      screen->esc_question = 0;
      for (i = 0; i < SCREEN_ESC_MAX; i++)
        screen->esc_params[i] = 0;
		} else {
			screen->state = PARSER_NORMAL;
		}
		break;

	case PARSER_CSI:
    if (c == '?') {
      if (screen->esc_cur_param == -1) {
        screen->esc_question = 1;
      } else {
        screen->state = PARSER_NORMAL;
      }
    } else {
      if (c >= '0' && c <= '9') {
        if (screen->esc_cur_param == -1)
          screen->esc_cur_param = 0;

        // Parse the current parameter
        if (screen->esc_cur_param < SCREEN_ESC_MAX)
          screen->esc_params[screen->esc_cur_param] = screen->esc_params[screen->esc_cur_param] * 10 + (c - '0');
      } else if (c == ';') {
        // Next parameter
        if (screen->esc_cur_param < SCREEN_ESC_MAX)
          screen->esc_cur_param++;
      } else {
        if (screen->esc_cur_param < SCREEN_ESC_MAX)
          screen->esc_cur_param++;

        screen_handle_esc(screen, c);
        screen->state = PARSER_NORMAL;
      }
    }
		break;

	default:
		screen->state = PARSER_NORMAL;
    break;
	}
}

void
screen_backspace(struct Screen *screen)
{
  if (screen->pos > 0)
    screen_set_char(screen, --screen->pos, ' ');
}
