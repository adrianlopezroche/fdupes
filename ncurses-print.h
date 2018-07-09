#pragma once

#define __USE_XOPEN
#include <wchar.h>
#define _XOPEN_SOURCE_EXTENDED
#include <ncursesw/ncurses.h>
#include <stdarg.h>

void putline(WINDOW *window, const char *str, const int line, const int columns, const int compensate_indent);
void print_spaces(WINDOW *window, int spaces);
void print_right_justified_int(WINDOW *window, int number, int width);
int vwprintflength(const wchar_t *format, va_list args);
int get_num_digits(int value);
