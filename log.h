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