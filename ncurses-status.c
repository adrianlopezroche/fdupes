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

#include "config.h"
#include <stdlib.h>
#include "ncurses-print.h"
#include "ncurses-status.h"

struct status_text *status_text_alloc(struct status_text *status, size_t width)
{
  struct status_text *result;
  wchar_t *newleft;
  wchar_t *newright;

  if (status == 0)
  {
    result = (struct status_text*) malloc(sizeof(struct status_text));
    if (result == 0)
      return 0;

    result->left = (wchar_t*) malloc((width+1) * sizeof(wchar_t));
    if (result->left == 0)
    {
      free(result);
      return 0;
    }

    result->right = (wchar_t*) malloc((width+1) * sizeof(wchar_t));
    if (result->right == 0)
    {
      free(result->left);
      free(result);
      return 0;
    }

    result->left[0] = '\0';
    result->right[0] = '\0';

    result->width = width;
  }
  else
  {
    if (status->width >= width)
      return status;

    newleft = (wchar_t*) realloc(status->left, (width+1) * sizeof(wchar_t));
    if (newleft == 0)
      return 0;

    newright = (wchar_t*) realloc(status->right, (width+1) * sizeof(wchar_t));
    if (newright == 0)
    {
      free(newleft);
      return 0;
    }

    result = status;
    result->left = newleft;
    result->right = newright;
    result->width = width;
  }

  return result;
}

void free_status_text(struct status_text *status)
{
  free(status->left);
  free(status->right);
  free(status);
}

void format_status_left(struct status_text *status, wchar_t *format, ...)
{
  va_list ap;
  va_list aq;
  int size;

  va_start(ap, format);
  va_copy(aq, ap);

  size = vwprintflength(format, aq);

  status_text_alloc(status, size);

  vswprintf(status->left, status->width + 1, format, ap);

  va_end(aq);
  va_end(ap);
}

void format_status_right(struct status_text *status, wchar_t *format, ...)
{
  va_list ap;
  va_list aq;
  int size;

  va_start(ap, format);
  va_copy(aq, ap);

  size = vwprintflength(format, aq);

  status_text_alloc(status, size);

  vswprintf(status->right, status->width + 1, format, ap);

  va_end(aq);
  va_end(ap);
}

void print_status(WINDOW *statuswin, struct status_text *status)
{
  wchar_t *text;
  size_t cols;
  size_t x;
  size_t l;

  cols = getmaxx(statuswin);

  text = (wchar_t*)malloc((cols + 1) * sizeof(wchar_t));

  l = wcslen(status->left);
  for (x = 0; x < l && x < cols; ++x)
    text[x] = status->left[x];
  for (x = l; x < cols; ++x)
    text[x] = L' ';

  l = wcslen(status->right);
  for (x = cols; x >= 1 && l >= 1; --x, --l)
    text[x - 1] = status->right[l - 1];

  text[cols] = L'\0';

  mvwaddnwstr(statuswin, 0, 0, text, wcslen(text));

  free(text);
}