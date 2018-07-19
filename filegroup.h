#ifndef FILEGROUP_H
#define FILEGROUP_H

#include "fdupes.h"

struct groupfile
{
  file_t *file;
  int action;
  int selected;
};

struct filegroup
{
  struct groupfile *files;
  size_t filecount;
  int startline;
  int endline;
  int selected;
};

#endif