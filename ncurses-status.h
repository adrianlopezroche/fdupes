/* FDUPES Copyright (c) 2018-2022 Adrian Lopez

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