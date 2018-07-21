#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "dir.h"

char *getworkingdirectory()
{
  size_t size;
  char *result;
  char *new_result;
  char *cwd;
  
  size = 1024;

  result = 0;
  do
  {
    new_result = (char*) realloc(result, size);
    if (new_result == 0)
    {
      if (result != 0)
        free(result);

      return 0;
    }

    result = new_result;

    cwd = getcwd(result, size);

    size *= 2;
  } while (cwd == 0 && errno == ERANGE);

  if (cwd == 0)
  {
    free(result);
    return 0;
  }

  return result;
}
