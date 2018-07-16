#ifndef NCURSESPRINT_H
#define NCURSESPRINT_H

#include <wchar.h>
#ifdef HAVE_NCURSESW_CURSES_H
  #include <ncursesw/curses.h>
#else
  #include <curses.h>
#endif
#include <stdarg.h>

void putline(WINDOW *window, const char *str, const int line, const int columns, const int compensate_indent);
void print_spaces(WINDOW *window, int spaces);
void print_right_justified_int(WINDOW *window, int number, int width);
int vwprintflength(const wchar_t *format, va_list args);
int get_num_digits(int value);

#endif