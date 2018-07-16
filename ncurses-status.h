#ifndef NCURSESSTATUS_H
#define NCURSESSTATUS_H

#include <wchar.h>
#ifdef HAVE_NCURSESW_CURSES_H
  #include <ncursesw/curses.h>
#else
  #include <curses.h>
#endif

struct status_text
{
  wchar_t *left;
  wchar_t *right;
  size_t width;
};

struct status_text *status_text_alloc(struct status_text *status, size_t width);
void free_status_text(struct status_text *status);
void format_status_left(struct status_text *status, wchar_t *format, ...);
void format_status_right(struct status_text *status, wchar_t *format, ...);
void print_status(WINDOW *statuswin, struct status_text *status);

#endif