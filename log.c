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

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fmatch.h"
#include "dir.h"
#include "log.h"

#define LOG_HEADER "[fdupes log]\n"

/* Open log file in append mode. If file exists, make sure it is a valid fdupes log file. 
*/
struct log_info *log_open(char *filename, int *error)
{
  struct log_info *info;
  int is_match;
  size_t read;

  info = (struct log_info*) malloc(sizeof(struct log_info));
  if (info == 0)
  {
    if (error != 0)
      *error = LOG_ERROR_OUT_OF_MEMORY;

    return 0;
  }

  info->file = fopen(filename, "a+");
  if (info->file == 0)
  {
    if (error != 0)
      *error = LOG_ERROR_FOPEN_FAILED;

    free(info);
    return 0;
  }

  fmatch(info->file, LOG_HEADER, &is_match, &read);
  if (!is_match && read > 0)
  {
    if (error != 0)
      *error = LOG_ERROR_NOT_A_LOG_FILE;

    free(info);
    return 0;
  }

  info->append = read > 0;

  info->log_start = 1;
  info->deleted = 0;
  info->remaining = 0;

  if (error != 0)
    *error = LOG_ERROR_NONE;

  return info;
}

/* Free linked lists holding set of deleted and remaining files.
*/
void log_free_set(struct log_info *info)
{
  struct log_file *f;
  struct log_file *next;

  f = info->deleted;
  while (f != 0)
  {
    next = f->next;

    free(f);

    f = next;
  }

  f = info->remaining;
  while (f != 0)
  {
    next = f->next;

    free(f);

    f = next;
  }

  info->deleted = 0;
  info->remaining = 0;
}

/* Signal beginning of duplicate set.
*/
void log_begin_set(struct log_info *info)
{
  log_free_set(info);
}

/* Add deleted file to log.
*/
int log_file_deleted(struct log_info *info, char *name)
{
  struct log_file *file;

  file = (struct log_file*) malloc(sizeof(struct log_file));
  if (file == 0)
    return 0;

  file->next = info->deleted;
  file->filename = name;

  info->deleted = file;

  return 1;
}

/* Add remaining file to log.
*/
int log_file_remaining(struct log_info *info, char *name)
{
  struct log_file *file;

  file = (struct log_file*) malloc(sizeof(struct log_file));
  if (file == 0)
    return 0;

  file->next = info->remaining;
  file->filename = name;

  info->remaining = file;

  return 1;
}

/* Output log header.
*/
void log_header(FILE *file)
{
  fprintf(file, "%s\n", LOG_HEADER);
}

/* Output log timestamp.
*/
void log_timestamp(FILE *file)
{
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);

  fprintf(file, "Log entry for %d-%02d-%02d %02d:%02d:%02d\n\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

/* Output current working directory.
*/
void log_cwd(FILE *file)
{
  char *cwd = getworkingdirectory();

  fprintf(file, "working directory:\n  %s\n\n", cwd);

  free(cwd);
}

/* Signal the end of a duplicate set.
*/
void log_end_set(struct log_info *info)
{
  struct log_file *f;

  if (info->deleted == 0)
    return;

  if (info->log_start)
  {
    if (info->append)
      fprintf(info->file, "---\n\n");
    else
      log_header(info->file);

    log_timestamp(info->file);
    log_cwd(info->file);

    info->log_start = 0;
  }

  f = info->deleted;
  do
  {
    fprintf(info->file, "deleted %s\n", f->filename);
    f = f->next;
  } while (f != 0);

  f = info->remaining;
  while (f != 0)
  {
    fprintf(info->file, "   left %s\n", f->filename);
    f = f->next;
  }

  fprintf(info->file, "\n");

  fflush(info->file);
}

/* Close log and free all memory.
*/
void log_close(struct log_info *info)
{
  fclose(info->file);

  log_free_set(info);

  free(info);
}
