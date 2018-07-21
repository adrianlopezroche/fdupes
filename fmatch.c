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
