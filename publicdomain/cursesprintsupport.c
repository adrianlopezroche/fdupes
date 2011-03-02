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

size_t marginprintw(int y, size_t leftmargin, size_t rightmargin, int wrapstyle, int quiet, char *text_in, ...)
{
  size_t linestart = 0;
  size_t lineend = 0;
  size_t maxwidth = rightmargin - leftmargin;
  int numlines = 0;
  size_t textend;
  va_list args;
  char text[512];
  int maxx;
  int maxy;

  getmaxyx(curscr, maxy, maxx);

  va_start(args, text_in);
  vsnprintf(text, 512, text_in, args);
  va_end(args);

  if (leftmargin >= rightmargin)
    return 0;

  textend = strlen(text);
 
  if (wrapstyle == MARGINPRINTW_WRAPCHARACTERS)
  {
    do {
      if (!quiet && y + numlines >= 0 && y + numlines < maxy)
	move(y + numlines, leftmargin);

      while (lineend < textend && lineend - linestart < maxwidth)
      {
	if (text[lineend] == '\n')
	{
	  ++numlines;
	  linestart = ++lineend;

	  if (!quiet && y + numlines >= 0 && y + numlines < maxy)
	    move(y + numlines, leftmargin);
      	
	  continue;
	}

	if (!quiet && y + numlines >= 0 && y + numlines < maxy)
	  addch(text[lineend]);
      
	++lineend;
      }
    
      if (lineend - linestart == maxwidth)
	++numlines;

      linestart = lineend;
    } while (lineend < textend);
  }

  return numlines;
}
