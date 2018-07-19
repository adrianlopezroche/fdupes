#ifndef WCS_H
#define WCS_H

#include <wchar.h>

int wcsmbcscmp(wchar_t *s1, char *s2);
int wcsinmbcs(char *haystack, wchar_t *needle);
int wcsbeginmbcs(char *haystack, wchar_t *needle);
int wcsendsmbcs(char *haystack, wchar_t *needle);
wchar_t *wcsrstr(wchar_t *haystack, wchar_t *needle);

#endif