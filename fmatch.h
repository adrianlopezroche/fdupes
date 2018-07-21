#ifndef FMATCH_H
#define FMATCH_H

#include <stdio.h>

void fmatch(FILE *file, char *matchstring, int *is_match, size_t *chars_read);

#endif
