/* cursesprintsupport.c

   This is free and unencumbered software released into the public domain.

   Anyone is free to copy, modify, publish, use, compile, sell, or
   distribute this software, either in source code form or as a compiled
   binary, for any purpose, commercial or non-commercial, and by any means.

   The author or authors of this software dedicate any and all copyright 
   interest in the software to the public domain. We make this dedication 
   for the benefit of the public at large and to the detriment of our heirs
   and successors. We intend this dedication to be an overt act of
   relinquishment in perpetuity of all present and future rights to this
   software under copyright law.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.
*/

#include "cursesprintsupport.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <ncurses.h>

size_t marginprintw(size_t leftmargin, size_t rightmargin, int quiet, char *text_in, ...)
{
  size_t linestart = 0;
  size_t lineend = 0;
  size_t maxwidth = rightmargin - leftmargin;
  size_t numlines = 0;
  size_t textend;
  int currentx;
  int currenty;
  va_list args;
  char text[512];

  va_start(args, text_in);
  vsnprintf(text, 512, text_in, args);
  va_end(args);

  if (leftmargin >= rightmargin)
    return 0;

  textend = strlen(text);

  getyx(stdscr, currenty, currentx);
  
  do {
    if (!quiet)
      move(currenty, leftmargin);

    while (lineend < textend && lineend - linestart < maxwidth)
    {
      if (text[lineend] == '\n')
      {
	++currenty;
	++numlines;
	linestart = ++lineend;

	if (!quiet)
	  move(currenty, leftmargin);
      	
	continue;
      }

      if (!quiet)
	addch(text[lineend]);
      
      ++lineend;
    }
    
    if (lineend - linestart == maxwidth)
    {
      ++currenty;
      ++numlines;
    }

    linestart = lineend;
  } while (lineend < textend);

  return numlines;
}
