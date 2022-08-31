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

#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#define LOG_ERROR_NONE 0
#define LOG_ERROR_OUT_OF_MEMORY 1
#define LOG_ERROR_FOPEN_FAILED 2
#define LOG_ERROR_NOT_A_LOG_FILE 3

struct log_file
{
  char *filename;
  struct log_file *next;
};

struct log_info
{
  FILE *file;
  int append;
  int log_start;
  struct log_file *deleted;
  struct log_file *remaining;
};

struct log_info *log_open(char *filename, int *error);
void log_begin_set(struct log_info *info);
int log_file_deleted(struct log_info *info, char *name);
int log_file_remaining(struct log_info *info, char *name);
void log_end_set(struct log_info *info);
void log_close(struct log_info *info);

#endif