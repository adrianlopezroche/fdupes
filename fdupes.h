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

#ifndef FDUPES_H
#define FDUPES_H

#include "config.h"
#include <sys/stat.h>
#include "md5/md5.h"

typedef struct _file {
  char *d_name;
  off_t size;
  md5_byte_t *crcpartial;
  md5_byte_t *crcsignature;
  dev_t device;
  ino_t inode;
  time_t mtime;
  time_t ctime;
  long mtime_nsec;
  long ctime_nsec;
  int hasdupes; /* true only if file is first on duplicate chain */
  struct _file *duplicates;
  struct _file *next;
} file_t;

#endif