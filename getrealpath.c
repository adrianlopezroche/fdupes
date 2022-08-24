/* Copyright (c) 2022 Adrian Lopez

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

#include "config.h"
#include "getrealpath.h"
#include "dir.h"
#include "sdirname.h"
#include "sbasename.h"
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define ISFLAG(a,b) ((a & b) == b)

#ifndef GETREALPATH_MAXSYMLINKS
#define GETREALPATH_MAXSYMLINKS 40
#endif

#define DEFAULT_LINK_ALLOCATION_SIZE 16

/* read link contents into buffer allocated via malloc() */
char *getlink(const char *path, struct stat *s)
{
    char *link;
    char *buffer;
    size_t allocated;
    int linksize;

    if (s->st_size > 0)
        allocated = s->st_size + 1;
    else
        allocated = DEFAULT_LINK_ALLOCATION_SIZE;

    buffer = malloc(allocated);
    if (buffer == 0)
        return 0;

    do
    {
        link = buffer;

        linksize = readlink(path, link, allocated);
        if (linksize == -1)
        {
            free(link);
            return 0;
        }

        if (linksize < allocated)
        {
            link[linksize] = '\0';
            return link;
        }

        if (stat(path, s) != 0)
        {
            free(link);
            return 0;
        }

        if (s->st_size != 0)
            allocated = s->st_size + 1;
        else
            allocated *= 2;
    } while (buffer = realloc(link, allocated));

    free(link);

    return 0;
}

/* replace dest[from .. through] with contents of src */
int replacestring(char *dest, size_t from, size_t through, size_t max, const char *src)
{
    size_t srclength;
    size_t destlength;
    size_t moveto;
    size_t newlength;

    destlength = strlen(dest);

    if (through >= destlength || from > through)
        return 0;

    srclength = strlen(src);

    newlength = destlength + srclength - (through - from + 1);
    if (newlength > max)
        return 0;

    memmove(dest + from + srclength, dest + through + 1, destlength - through);

    memcpy(dest + from, src, srclength);

    return 1;
}

/* print the resolved absolute file name for the specified path */
char *getrealpath(const char *path, unsigned int options)
{
    char *scratch;
    char *cwd;
    char *link;
    char *newmem;
    char *dirname;
    char *basename;
    char save;
    size_t tail;
    size_t next;
    size_t pathlength;
    size_t linklength;
    size_t cwdlength;
    size_t allocated;
    size_t links;
    size_t x;
    struct stat st;
    struct stat st0;
    int pathexists;

    /* run stat on unmodified path, for later use */
    pathexists = stat(path, &st0) == 0;
    if (!pathexists && !ISFLAG(options, GETREALPATH_IGNORE_MISSING_BASENAME))
        return 0;

    /* optionally ignore the last component if it does not exist */
    if (ISFLAG(options, GETREALPATH_IGNORE_MISSING_BASENAME) && !pathexists)
    {
        dirname = malloc(strlen(path) + 1);
        if (dirname == 0)
            return 0;

        basename = malloc(strlen(path) + 1);
        if (basename == 0)
            return 0;

        sdirname(dirname, path);
        sbasename(basename, path);

        if (stat(dirname, &st0) != 0)
            return 0;

        link = getrealpath(dirname, options ^ GETREALPATH_IGNORE_MISSING_BASENAME);
        if (link == 0)
        {
            free(basename);
            free(dirname);
            return 0;
        }

        scratch = malloc(strlen(link) + strlen(basename) + 1);
        if (scratch == 0)
        {
            free(basename);
            free(dirname);
            return 0;
        }

        strcpy(scratch, link);
        strcat(scratch, "/");
        strcat(scratch, basename);

        if (stat(dirname, &st) != 0)
        {
            free(link);
            free(basename);
            free(dirname);
            free(scratch);
            return 0;
        }

        free(link);
        free(basename);
        free(dirname);

        if
        (
        st.st_dev != st0.st_dev ||
        st.st_ino != st0.st_ino
        )
        {
            free(scratch);
            return 0;
        }

        return scratch;
    }

    if (path[0] == '/')
    /* if path is an absolute path, copy its contents to scratch buffer */
    {
        allocated = strlen(path) + 1;

        scratch = malloc(allocated);
        if (scratch == 0)
            return 0;

        memcpy(scratch, path, allocated);
    }
    else
    /* if path is a relative path, combine cwd and path into scratch buffer */
    {
        cwd = getworkingdirectory();
        if (cwd == 0)
            return 0;

        pathlength = strlen(path);
        cwdlength = strlen(cwd);

        allocated = pathlength + cwdlength + 2;

        scratch = malloc(allocated);
        if (scratch == 0)
        {
            free(cwd);
            return 0;
        }

        memcpy(scratch, cwd, cwdlength);

        scratch[cwdlength] = '/';

        memcpy(scratch + cwdlength + 1, path, pathlength + 1);

        free(cwd);
    }

    tail = 0;
    next = 0;
    links = 0;

    while (scratch[next] != '\0')
    {
        if (scratch[next] == '/')
        /* advance to start of filename */
        {
            /* keep the first slash */
            scratch[tail++] = scratch[next++];

            /* skip the rest */
            while (scratch[next] == '/')
                ++next;

            continue;
        }
        else if (scratch[next] == '.')
        /* handle filenames beginning with "." */
        {
            switch (scratch[next + 1])
            {
                case '/':
                    /* collapse /./ down to / */
                    do
                        ++next;
                    while (scratch[next] == '/');

                    continue;

                case '\0':
                    /* truncate trailing /. down to / */
                    ++next;
                    continue;

                case '.':
                    if (scratch[next + 2] == '/')
                    /* go up one directory from /../ */
                    {
                        if (tail > 1)
                            tail -= 2;
                        else
                            tail = 0;

                        while (scratch[tail] != '/')
                            --tail;

                        if (tail == 0)
                        {
                            tail = 1;
                            ++next;
                        }
                        else
                        {
                            next += 2;
                        }
                    }
                    else if (scratch[next + 2] == '\0')
                    /* go up one directory from trailing /.. */
                    {
                        if (tail > 1)
                            tail -= 2;
                        else
                            tail = 0;

                        while (scratch[tail] != '/')
                            --tail;

                        if (tail == 0)
                        {
                            tail = 1;
                            ++next;
                        }
                        else
                        {
                            next += 2;
                        }
                    }
                    else
                    /* process .. of regular filename beginning with .. */
                    {
                        do
                            scratch[tail++] = scratch[next++];
                        while (scratch[next] == '.');
                    }

                    break;

                default:
                    /* process . of regular filename begining with . */
                    scratch[tail++] = scratch[next++];
                    break;
            }
        }
        else
        /* process regular filename characters */
        {
            do
                scratch[tail++] = scratch[next++];
            while (scratch[next] != '\0' && scratch[next] != '/');
        }

        save = scratch[tail];

        scratch[tail] = '\0';

        if (lstat(scratch, &st) != 0)
        {
            free(scratch);
            return 0;
        }

        if (S_ISLNK(st.st_mode))
        {
            if (links++ > GETREALPATH_MAXSYMLINKS)
            {
                free(scratch);
                return 0;
            }

            link = getlink(scratch, &st);
            if (link == 0)
            {
                free(scratch);
                return 0;
            }

            pathlength = strlen(scratch) + strlen(scratch + next);

            linklength = strlen(link);

            if (pathlength + linklength + 1 > allocated)
            {
                allocated += pathlength + linklength + 1;

                newmem = realloc(scratch, allocated);
                if (newmem == 0)
                {
                    free(scratch);
                    return 0;
                }

                scratch = newmem;
            }

            scratch[tail] = save;

            if (link[0] == '/')
            /* link represents an absolute path */
            {
                memmove(scratch + tail, scratch + next, strlen(scratch + next) + 1);

                replacestring(scratch, 0, tail - 1, allocated, link);

                /* start over */
                tail = 0;
                next = 0;
            }
            else
            /* link represents a relative path */
            {
                memmove(scratch + tail, scratch + next, strlen(scratch + next) + 1);

                x = tail;

                while (scratch[x] != '/')
                    --x;

                replacestring(scratch, x + 1, tail - 1, allocated, link);

                tail = x;
                next = x;
            }

            free(link);
        }
        else
        {
            scratch[tail] = save;
        }
    }

    /* terminate path */
    if (tail > 1 && scratch[tail - 1] == '/')
        scratch[tail - 1] = '\0';
    else
        scratch[tail] = '\0';

    /* confirm that scratch and path both point to the same file */
    if (stat(scratch, &st) != 0)
    {
        free(scratch);
        return 0;
    }

    if
    (
    st.st_dev != st0.st_dev ||
    st.st_ino != st0.st_ino
    )
    {
        free(scratch);
        return 0;
    }

    return scratch;
}
