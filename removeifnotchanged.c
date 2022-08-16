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
