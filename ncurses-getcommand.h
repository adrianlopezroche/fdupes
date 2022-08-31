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

#ifndef NCURSESGETCOMMAND_H
#define NCURSESGETCOMMAND_H

#include <wchar.h>
#ifdef HAVE_NCURSESW_CURSES_H
  #include <ncursesw/curses.h>
#else
  #include <curses.h>
#endif
#include "ncurses-prompt.h"

#define GET_COMMAND_OK 1
#define GET_COMMAND_CANCELED 2
#define GET_COMMAND_ERROR_OUT_OF_MEMORY 3
#define GET_COMMAND_RESIZE_REQUESTED 4
#define GET_COMMAND_KEY_NPAGE 5
#define GET_COMMAND_KEY_PPAGE 6
#define GET_COMMAND_KEY_SF 7
#define GET_COMMAND_KEY_SR 8

void get_command_arguments(wchar_t **arguments, wchar_t *input);
int get_command_text(wchar_t **commandbuffer, size_t *commandbuffersize, WINDOW *promptwin, struct prompt_info *prompt, int cancel_on_erase, int append);

#endif