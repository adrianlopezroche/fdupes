/* truefilename.c

   This is free and unencumbered software released into the public domain.

   Anyone is free to copy, modify, publish, use, compile, sell, or
   distribute this software, either in source code form or as a compiled
   binary, for any purpose, commercial or non-commercial, and by any means.

   The author or authors of this software dedicate any and all copyright 
   interest in the software to the public domain. We make this dedication 
   for the benefit of the public at large and to the detriment of our heirs
   and successors. We intend this dedication to be an overt act of
   relinquishment in perpetuity of all present and future rights to this
   software under copyright law.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.
*/

#include "truefilename.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>

// debug:
#include <stdio.h>

/* resolve any symlinks along path */
string_t *truefilename(char *path_in)
{
  string_t *currentpath;
  string_t *finalpath;
  string_t *tempstring;
  struct stat linfo;
  ssize_t read;
  int up;

  currentpath = string_init();
  finalpath = string_init();
  tempstring = string_init();
  up = 0;
  
  if (path_in[0] != '/')
  {
    while (!getcwd(currentpath->buffer, currentpath->buffersize))
    {
      if (errno == ERANGE)
	string_reserve(currentpath, currentpath->buffersize * 2);
      else
	;
    }

    string_append_chars(currentpath, "/");
  }

  string_append_chars(currentpath, path_in);

  while (!(currentpath->buffer[0] == '.' && currentpath->buffer[1] == '\0') && 
	 !(currentpath->buffer[0] == '/' && currentpath->buffer[1] == '\0'))
  {
    printf("[\"%s\", \"%s\"]\n", currentpath->buffer, finalpath->buffer);

    lstat(currentpath->buffer, &linfo);
    if (S_ISLNK(linfo.st_mode))
    {
      read = readlink(currentpath->buffer, tempstring->buffer, tempstring->buffersize);            
      while (read == tempstring->buffersize)
      {
	string_reserve(tempstring, tempstring->buffersize * 2);
	read = readlink(currentpath->buffer, tempstring->buffer, tempstring->buffersize);
      }
      tempstring->buffer[read] = '\0';
      
      if (tempstring->buffer[0] == '/')
      {
	string_copy_chars(currentpath, tempstring->buffer);
      }
      else
      {
	string_copy_chars(currentpath, dirname(currentpath->buffer));
	string_append_chars(currentpath, "/");
	string_append_chars(currentpath, tempstring->buffer);
      }
    }
    else
    {
      string_copy_chars(tempstring, basename(currentpath->buffer));
      if (tempstring->buffer[0] == '.' && tempstring->buffer[1] == '\0')
      {
	if (up)
	  --up;

	string_copy_chars(currentpath, dirname(currentpath->buffer));

	continue;
      }
      else
	if (tempstring->buffer[0] == '.' && tempstring->buffer[1] == '.' && tempstring->buffer[2] == '\0')
	{
	  ++up;
	  string_copy_chars(currentpath, dirname(currentpath->buffer));
	  continue;
	}
	else
	  if (up)
	  {
	    --up;
	      
	    string_copy_chars(currentpath, dirname(currentpath->buffer));
	      
	    continue;
	  }

      string_copy_chars(tempstring, finalpath->buffer);
      string_copy_chars(finalpath, basename(currentpath->buffer));
      
      if (tempstring->buffer[0] != '\0')
      {
	string_append_chars(finalpath, "/");
	string_append_chars(finalpath, tempstring->buffer);
      }

      string_copy_chars(currentpath, dirname(currentpath->buffer));
    }
  }

  if (currentpath->buffer[0] == '/')
  {
    string_copy_chars(tempstring, finalpath->buffer);
    string_copy_chars(finalpath, "/");
    string_append_chars(finalpath, tempstring->buffer);
  }
 
  printf("[\"%s\", \"%s\"]\n\n", currentpath->buffer, finalpath->buffer);

  string_free(tempstring);
  string_free(currentpath);

  return finalpath;
}
