/* FDUPES Copyright (c) 2018 Adrian Lopez

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation files
   (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "config.h"
#include <stdlib.h>
#include <wchar.h>
#include "ncurses-print.h"
#include "errormsg.h"
#include "mbstowcs_escape_invalid.h"
#include "positive_wcwidth.h"

void putline(WINDOW *window, const char *str, const int line, const int columns, const int compensate_indent)
{
  wchar_t *dest = 0;
  int inputlength;
  int linestart;
  int linelength;
  int linewidth;
  int first_line_columns;
  int l;

  inputlength = mbstowcs_escape_invalid(0, str, 0);
  if (inputlength == 0)
    return;

  dest = (wchar_t *) malloc((inputlength + 1) * sizeof(wchar_t));
  if (dest == NULL)
  {
    endwin();
    errormsg("out of memory\n");
    exit(1);
  }

  mbstowcs_escape_invalid(dest, str, inputlength);
  dest[inputlength] = L'\0';

  first_line_columns = columns - compensate_indent;

  linestart = 0;

  if (line > 0)
  {
    linewidth = positive_wcwidth(dest[linestart]);

    while (linestart + 1 < inputlength && linewidth + positive_wcwidth(dest[linestart + 1]) <= first_line_columns)
      linewidth += positive_wcwidth(dest[++linestart]);

    if (++linestart == inputlength)
      return;

    for (l = 1; l < line; ++l)
    {
      linewidth = positive_wcwidth(dest[linestart]);

      while (linestart + 1 < inputlength && linewidth + positive_wcwidth(dest[linestart + 1]) <= columns)
        linewidth += positive_wcwidth(dest[++linestart]);

      if (++linestart == inputlength)
        return;
    }
  }

  linewidth = positive_wcwidth(dest[linestart]);
  linelength = 1;

  if (line == 0)
  {
    while (linestart + linelength < inputlength && linewidth + positive_wcwidth(dest[linestart + linelength]) <= first_line_columns)
    {
      linewidth += positive_wcwidth(dest[linestart + linelength]);
      ++linelength;
    }
  }
  else
  {
    while (linestart + linelength < inputlength && linewidth + positive_wcwidth(dest[linestart + linelength]) <= columns)
    {
      linewidth += positive_wcwidth(dest[linestart + linelength]);
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
