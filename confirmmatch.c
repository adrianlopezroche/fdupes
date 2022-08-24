/* FDUPES Copyright (c) 1999-2022 Adrian Lopez

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
#include "sigint.h"
#include "confirmmatch.h"
#include <stdlib.h>
#include <memory.h>

/* Do a bit-for-bit comparison in case two different files produce the
   same signature. Unlikely, but better safe than sorry. */

int confirmmatch(FILE *file1, FILE *file2)
{
  unsigned char c1[CHUNK_SIZE];
  unsigned char c2[CHUNK_SIZE];
  size_t r1;
  size_t r2;

  fseek(file1, 0, SEEK_SET);
  fseek(file2, 0, SEEK_SET);

  do {
    if (got_sigint) {
      fclose(file1);
      fclose(file2);
      exit(0);
    }

    r1 = fread(c1, sizeof(unsigned char), sizeof(c1), file1);
    r2 = fread(c2, sizeof(unsigned char), sizeof(c2), file2);

    if (r1 != r2) return 0; /* file lengths are different */
    if (memcmp (c1, c2, r1)) return 0; /* file contents are different */
  } while (r2);

  return 1;
}
