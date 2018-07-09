#include <stdlib.h>
#define __USE_XOPEN
#include <wchar.h>
#include "ncurses-print.h"
#include "errormsg.h"

void putline(WINDOW *window, const char *str, const int line, const int columns, const int compensate_indent)
{
  wchar_t *dest = 0;
  int inputlength;
  int linestart;
  int linelength;
  int linewidth;
  int first_line_columns;
  int l;

  inputlength = mbstowcs(0, str, 0);
  if (inputlength == 0)
    return;

  dest = (wchar_t *) malloc((inputlength + 1) * sizeof(wchar_t));
  if (dest == NULL)
  {
    endwin();
    errormsg("out of memory\n");
    exit(1);
  }

  mbstowcs(dest, str, inputlength);
  dest[inputlength] = L'\0';

  first_line_columns = columns - compensate_indent;

  linestart = 0;

  if (line > 0)
  {
    linewidth = wcwidth(dest[linestart]);

    while (linestart + 1 < inputlength && linewidth + wcwidth(dest[linestart + 1]) <= first_line_columns)
      linewidth += wcwidth(dest[++linestart]);

    if (++linestart == inputlength)
      return;

    for (l = 1; l < line; ++l)
    {
      linewidth = wcwidth(dest[linestart]);

      while (linestart + 1 < inputlength && linewidth + wcwidth(dest[linestart + 1]) <= columns)
        linewidth += wcwidth(dest[++linestart]);

      if (++linestart == inputlength)
        return;
    }
  }

  linewidth = wcwidth(dest[linestart]);
  linelength = 1;

  if (line == 0)
  {
    while (linestart + linelength < inputlength && linewidth + wcwidth(dest[linestart + linelength]) <= first_line_columns)
    {
      linewidth += wcwidth(dest[linestart + linelength]);
      ++linelength;
    }
  }
  else
  {
    while (linestart + linelength < inputlength && linewidth + wcwidth(dest[linestart + linelength]) <= columns)
    {
      linewidth += wcwidth(dest[linestart + linelength]);
      ++linelength;
    }    
  }

  waddnwstr(window, dest + linestart, linelength);

  free(dest);
}

void print_spaces(WINDOW *window, int spaces)
{
  int x;

  for (x = 0; x < spaces; ++x)
    waddch(window, L' ');
}

void print_right_justified_int(WINDOW *window, int number, int width)
{
  int length;

  length = get_num_digits(number);
  if (number < 0)
    ++length;

  if (length < width)
    print_spaces(window, width - length);

  wprintw(window, "%d", number);
}

int vwprintflength(const wchar_t *format, va_list args)
{
  FILE *fp;
  int size;

  fp = fopen("/dev/null", "w");
  if (fp == 0)
    return 0;

  size = vfwprintf(fp, format, args);

  fclose(fp);

  return size;
}

int get_num_digits(int value)
{
  int digits = 0;

  if (value < 0)
    value = -value;

  do {
    value /= 10;
    ++digits;
  } while (value > 0);

  return digits;
}
