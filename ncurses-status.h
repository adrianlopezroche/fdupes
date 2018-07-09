#pragma once

#define __USE_XOPEN
#include <wchar.h>
#define _XOPEN_SOURCE_EXTENDED
#include <ncursesw/ncurses.h>

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