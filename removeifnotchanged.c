/* FDUPES Copyright (c) 2022 Adrian Lopez

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
#include "removeifnotchanged.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>

int removeifnotchanged(const file_t *file, char **errorstring)
{
  int result;
  struct stat st;

  static char *filechanged = "File contents changed during processing";
  static char *unknownerror = "Unknown error";

  stat(file->d_name, &st);

  if (file->device != st.st_dev ||
      file->inode != st.st_ino ||
      file->ctime != st.st_ctime ||
      file->mtime != st.st_mtime ||
#ifdef HAVE_NSEC_TIMES
      file->ctime_nsec != st.st_ctim.tv_nsec ||
      file->mtime_nsec != st.st_mtim.tv_nsec ||
#endif
      file->size != st.st_size)
  {
    if (errorstring != 0)
        *errorstring = filechanged;

    return -2;
  }
  else
  {
    result = remove(file->d_name);

    if (result != 0 && errorstring != 0)
    {
      *errorstring = strerror(errno);

      if (*errorstring == 0)
        *errorstring = unknownerror;
    }

    return result;
  }
}
