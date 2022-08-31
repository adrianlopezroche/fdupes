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

#ifndef NCURSESPROMPT_H
#define NCURSESPROMPT_H

#include <wchar.h>
#ifdef HAVE_NCURSESW_CURSES_H
  #include <ncursesw/curses.h>
#else
  #include <curses.h>
#endif

struct prompt_info
{
  wchar_t *text;
  size_t allocated;
  size_t offset;
  size_t cursor;
  int active;
};

struct prompt_info *prompt_info_alloc(size_t initial_size);
void free_prompt_info(struct prompt_info *info);
int format_prompt(struct prompt_info *prompt, wchar_t *format, ...);
void set_prompt_active_state(struct prompt_info *prompt, int active);
void update_prompt(WINDOW *promptwin, struct prompt_info *prompt, wchar_t *commandbuffer, int cursor_delta);
void print_prompt(WINDOW *promptwin, struct prompt_info *prompt, wchar_t *commandbuffer);

#endif