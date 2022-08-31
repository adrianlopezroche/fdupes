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
