#include <stdio.h>
#include <stdarg.h>
#include "errormsg.h"

void errormsg(char *message, ...)
{
  va_list ap;

  va_start(ap, message);

  fprintf(stderr, "\r%40s\r%s: ", "", PROGRAM_NAME);
  vfprintf(stderr, message, ap);
}
