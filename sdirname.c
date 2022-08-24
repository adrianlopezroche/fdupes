/* The contents of this file are in the public domain.
*/
#include "sdirname.h"
#include <string.h>
#include <libgen.h>
#include <stdlib.h>

char *sdirname(char *str, const char *path)
{
    char *name;

    if (str == 0)
    {
        str = malloc(strlen(path) + 1);
        if (str == 0)
            return 0;
    }

    strcpy(str, path);

    name = dirname(str);

    memmove(str, name, strlen(name) + 1);

    return str;
}