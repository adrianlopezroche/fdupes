/* fgetline.c

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

#include "fgetline.h"
#include <errno.h>

char *fgetline(FILE *file, string_t *buffer)
{
  int done;
  size_t stringlength;

  stringlength = 0;

  int c = EOF;
  done = 0;
  do
  {
    if (!string_reserve(buffer, stringlength + 1))
    {
      errno = FGETLINE_OUTOFMEMORY;
      string_free(buffer);
      return 0;
    }

    c = fgetc(file);
    if (c == EOF || c == '\n')
      done = 1;
    else
      buffer->buffer[stringlength++] = c;
  } while (!done);

  buffer->buffer[stringlength] = '\0';
  
  if (c == EOF)
  {
    errno = FGETLINE_EOF;
    return 0;
  }
  else  
    return buffer->buffer;
}
