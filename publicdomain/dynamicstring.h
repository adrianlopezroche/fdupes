/* dynamicstring.h

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

#ifndef DYNAMICSTRING_H
#define DYNAMICSTRING_H

#include <unistd.h>

typedef struct _string
{
  char *buffer;
  int buffersize;
} string_t;

string_t *string_init();
void string_free(string_t *buffer);

int string_reserve(string_t *buffer, size_t reserve);
int string_copy_chars(string_t *string, char *chars);
int string_append_chars(string_t *string, char *chars);

#endif
