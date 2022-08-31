/* Copyright (c) 2019-2022 Adrian Lopez

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
#include "mbstowcs_escape_invalid.h"
#include <wchar.h>
#include <string.h>
#include <stdio.h>

void reset_mbstate(mbstate_t *state)
{
    memset(state, 0, sizeof(mbstate_t));
}

size_t putoctal(wchar_t *dest, char c)
{
    swprintf(dest, 5, L"\\%03o", (unsigned char) c);
    return 4;
}

size_t put_invalid_sequence(wchar_t *dest, const char *src, size_t *destination_index, size_t count, size_t max)
{
    size_t x;

    for (x = 0; x < count; ++x)
    {
        if (dest != 0)
        {
            if (*destination_index + 5 > max)
                return x;

            putoctal(dest + *destination_index, src[x]);
        }

        *destination_index += 4;
    }

    return count;
}

size_t mbstowcs_escape_invalid(wchar_t *dest, const char *src, size_t n)
{
    mbstate_t state;
    wchar_t wc;
    size_t x;
    size_t i;
    size_t dx;
    size_t write;
    size_t written;
    size_t result;

    reset_mbstate(&state);

    x = 0;
    i = 0;
    dx = 0;

    while (src[x] != '\0')
    {
        result = mbrtowc(&wc, src + x, 1, &state);

        if (result == -2)
        /* sequence is not yet complete */
        {
            ++x;
            ++i;
        }
        else if (result == -1)
        /* invalid sequence */
        {
            write = i == 0 ? 1 : i;

            if (dest != 0)
            {
                written = put_invalid_sequence(dest, src + (x - i), &dx, write, n);

                if (written != write)
                    return -1;
            }
            else
                put_invalid_sequence(0, src + (x - i), &dx, write, 0);

            if (i == 0)
                ++x;
    
            i = 0;

            reset_mbstate(&state);
        }
        else if (result != 0)
        /* OK, add character */
        {
            if (dest != 0)
            {
                if (dx < n)
                    dest[dx++] = wc;
                else
                    return -1;
            }
            else
                ++dx;

            ++x;

            i = 0;
        }
        
        if (src[x] == L'\0' && i > 0)
        /* output final incomplete sequence */
        {
            if (dest != 0)
            {
                written = put_invalid_sequence(dest, src + (x - i), &dx, i, n);

                if (written != i)
                    return -1;
            }
            else
                put_invalid_sequence(0, src + (x - i), &dx, i, 0);
        }        
    }

    if (dest != 0)
    {
        if (dx < n)
            dest[dx] = L'\0';
        else
            return -1;
    }

    return dx + 1;
}
