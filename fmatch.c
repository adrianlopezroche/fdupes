/* Copyright (c) 2018-2022 Adrian Lopez

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
      claim that you wrote the original software. If you use this software
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.
   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original software.
   3. This notice may not be removed or altered from any source distribution. */

#include <string.h>
#include "fmatch.h"

/* Test whether given string matches text at current file position.
*/
void fmatch(FILE *file, char *matchstring, int *is_match, size_t *chars_read)
{
  size_t len;
  int x;
  int c;

  *is_match = 0;
  *chars_read = 0;

  len = strlen(matchstring);
  for (x = 0; x < len; ++x)
  {
    c = fgetc(file);
    if (c == EOF)
      return;

    (*chars_read)++;

    if ((char)c != matchstring[x])
      return;
  }

  *is_match = 1;
}
