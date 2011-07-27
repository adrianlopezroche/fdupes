/* dynamicstring.c

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

#include "dynamicstring.h"
#include <stdlib.h>
#include <string.h>

#ifndef DYNAMICSTRING_INIT_SIZE
#define DYNAMICSTRING_INIT_SIZE 32
#endif

string_t *string_init()
{
  string_t *buffer;

  buffer = (string_t*) malloc(sizeof(string_t));
  if (!buffer)
    return 0;

  buffer->buffersize = DYNAMICSTRING_INIT_SIZE;
  buffer->buffer = (char*) malloc(buffer->buffersize);

  if (!buffer->buffer)
  {
    free(buffer);
    return 0;
  }

  buffer->buffer[0] = '\0';

  return buffer;
}

void string_free(string_t *buffer)
{
  if (buffer)
  {
    free(buffer->buffer);
    free(buffer);
  }
}

int string_reserve(string_t *buffer, size_t reserve)
{
  char *newbuffer;
  size_t newbuffersize;

  if (!buffer)
    return 0;

  if (reserve <= buffer->buffersize)
    return 1;

  newbuffersize = buffer->buffersize;
  while (newbuffersize < reserve)
    newbuffersize *= 2;

  newbuffer = realloc(buffer->buffer, newbuffersize);

  if (!newbuffer)
    return 0;

  buffer->buffer = newbuffer;
  buffer->buffersize = newbuffersize;

  return 1;
}

int string_copy_chars(string_t *string, char *chars)
{
  if (!string_reserve(string, strlen(chars) + 1))
    return 0;

  strcpy(string->buffer, chars);

  return 1;
}

int string_append_chars(string_t *string, char *chars)
{
  size_t totalsize;

  totalsize = strlen(string->buffer) + strlen(chars) + 1;

  if (!string_reserve(string, totalsize))
    return 0;

  strcat(string->buffer, chars);

  return 1;
}
