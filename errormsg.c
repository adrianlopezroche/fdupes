#include <config.h>
#include <stdio.h>
#include <stdarg.h>
#include "errormsg.h"

extern char *program_name;

void errormsg(char *message, ...)
{
  va_list ap;

  va_start(ap, message);

  fprintf(stderr, "\r%40s\r%s: ", "", program_name);
  vfprintf(stderr, message, ap);
}
