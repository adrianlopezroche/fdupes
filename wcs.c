/* Copyright (c) 2018 Adrian Lopez

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

#include <stdlib.h>
#include "wcs.h"
#include "mbstowcs_escape_invalid.h"

/* compare wide and multibyte strings */
int wcsmbcscmp(wchar_t *s1, char *s2)
{
  wchar_t *s2w;
  size_t size;
  int result;

  size = mbstowcs_escape_invalid(0, s2, 0) + 1;

  s2w = (wchar_t*) malloc(size * sizeof(wchar_t));
  if (s2w == 0)
    return -1;

  mbstowcs_escape_invalid(s2w, s2, size);

  result = wcscmp(s1, s2w);

  free(s2w);

  return result;
}

/* wide character needle is contained in multibyte haystack */
int wcsinmbcs(char *haystack, wchar_t *needle)
{
  wchar_t *haystackw;
  size_t size;
  int result;

  size = mbstowcs_escape_invalid(0, haystack, 0) + 1;

  haystackw = (wchar_t*) malloc(size * sizeof(wchar_t));
  if (haystackw == 0)
    return -1;

  mbstowcs_escape_invalid(haystackw, haystack, size);

  if (wcsstr(haystackw, needle) == 0)
    result = 0;
  else
    result = 1;

  free(haystackw);

  return result;
}

/* wide character needle at beginning of multibyte haystack */
int wcsbeginmbcs(char *haystack, wchar_t *needle)
{
  wchar_t *haystackw;
  size_t size;
  int result;

  size = mbstowcs_escape_invalid(0, haystack, 0);

  haystackw = (wchar_t*) malloc(size * sizeof(wchar_t));
  if (haystackw == 0)
    return -1;

  mbstowcs_escape_invalid(haystackw, haystack, size);

  if (wcsncmp(haystackw, needle, wcslen(needle)) == 0)
    result = 1;
  else
    result = 0;

  free(haystackw);

  return result;
}

/* wide character needle at end of multibyte haystack */
int wcsendsmbcs(char *haystack, wchar_t *needle)
{
  wchar_t *haystackw;
  size_t size;
  int result;

  size = mbstowcs_escape_invalid(0, haystack, 0) + 1;

  haystackw = (wchar_t*) malloc(size * sizeof(wchar_t));
  if (haystackw == 0)
    return -1;

  mbstowcs_escape_invalid(haystackw, haystack, size);

  if (wcsrstr(haystackw, needle) != 0 && wcscmp(wcsrstr(haystackw, needle), needle) == 0)
    result = 1;
  else
    result = 0;

  free(haystackw);

  return result;
}

/* wide character reverse string search */
wchar_t *wcsrstr(wchar_t *haystack, wchar_t *needle)
{
  wchar_t *found = 0;
  wchar_t *next = 0;

  found = wcsstr(haystack, needle);
  if (found)
  {
    do {
      next = wcsstr(found + 1, needle);
      if (next != 0)
        found = next;
    } while (next);

    return found;
  }

  return 0;
}