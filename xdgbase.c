/* The contents of this file are in the public domain.
*/
#include "config.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>

#define XDG_CACHE_HOME_BASENAME ".cache"
#define XDG_CACHE_HOME_PERMISSIONS 0700

char *getcachehome(int createdefault)
{
    char *cachedir;
    char *homedir;
    char *result;
    size_t pathlength;
    size_t defaultdirlength;
    struct passwd *pw;
    struct stat st;

    cachedir = getenv("XDG_CACHE_HOME");
    if (cachedir != 0)
    {
        pathlength = strlen(cachedir);

        result = malloc(pathlength + 1);
        if (result == 0)
            return 0;

        strcpy(result, cachedir);
    }
    else
    {
        homedir = getenv("HOME");
        if (homedir == 0)
        {
            pw = getpwuid(getuid());
            if (pw == 0)
                return 0;

            homedir = pw->pw_dir;
        }

        if (homedir == 0)
            return 0;

        pathlength = strlen(homedir);

        defaultdirlength = strlen(XDG_CACHE_HOME_BASENAME);

        result = malloc(pathlength + defaultdirlength + 2);
        if (result == 0)
            return 0;

        memmove(result, homedir, pathlength);
        memmove(result + pathlength + 1, XDG_CACHE_HOME_BASENAME, defaultdirlength);

        result[pathlength] = '/';
        result[pathlength + defaultdirlength + 1] = '\0';

        if (createdefault && stat(result, &st) != 0)
            mkdir(result, XDG_CACHE_HOME_PERMISSIONS);
    }

    return result;
}