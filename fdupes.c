/* FDUPES Copyright (c) 1999-2002 Adrian Lopez

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation files
   (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#ifndef OMIT_GETOPT_LONG
#include <getopt.h>
#endif
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <locale.h>
#define __USE_XOPEN
#include <wchar.h>
#define _XOPEN_SOURCE_EXTENDED
#include <ncursesw/ncurses.h>
#include <pcre2.h>

#define KEY_ESCAPE 27

#ifndef EXTERNAL_MD5
#include "md5/md5.h"
#endif

#define ISFLAG(a,b) ((a & b) == b)
#define SETFLAG(a,b) (a |= b)

#define F_RECURSE           0x0001
#define F_HIDEPROGRESS      0x0002
#define F_DSAMELINE         0x0004
#define F_FOLLOWLINKS       0x0008
#define F_DELETEFILES       0x0010
#define F_EXCLUDEEMPTY      0x0020
#define F_CONSIDERHARDLINKS 0x0040
#define F_SHOWSIZE          0x0080
#define F_OMITFIRST         0x0100
#define F_RECURSEAFTER      0x0200
#define F_NOPROMPT          0x0400
#define F_SUMMARIZEMATCHES  0x0800

char *program_name;

unsigned long flags = 0;

static volatile int got_sigint = 0;

#define CHUNK_SIZE 8192

#define INPUT_SIZE 256

#define PARTIAL_MD5_SIZE 4096

/* 

TODO: Partial sums (for working with very large files).

typedef struct _signature
{
  md5_state_t state;
  md5_byte_t  digest[16];
} signature_t;

typedef struct _signatures
{
  int         num_signatures;
  signature_t *signatures;
} signatures_t;

*/

typedef struct _file {
  char *d_name;
  off_t size;
  char *crcpartial;
  char *crcsignature;
  dev_t device;
  ino_t inode;
  time_t mtime;
  int hasdupes; /* true only if file is first on duplicate chain */
  struct _file *duplicates;
  struct _file *next;
} file_t;

typedef struct _filetree {
  file_t *file; 
  struct _filetree *left;
  struct _filetree *right;
} filetree_t;

void errormsg(char *message, ...)
{
  va_list ap;

  va_start(ap, message);

  fprintf(stderr, "\r%40s\r%s: ", "", program_name);
  vfprintf(stderr, message, ap);
}

void escapefilename(char *escape_list, char **filename_ptr)
{
  int x;
  int tx;
  char *tmp;
  char *filename;

  filename = *filename_ptr;

  tmp = (char*) malloc(strlen(filename) * 2 + 1);
  if (tmp == NULL) {
    errormsg("out of memory!\n");
    exit(1);
  }

  for (x = 0, tx = 0; x < strlen(filename); x++) {
    if (strchr(escape_list, filename[x]) != NULL) tmp[tx++] = '\\';
    tmp[tx++] = filename[x];
  }

  tmp[tx] = '\0';

  if (x != tx) {
    *filename_ptr = realloc(*filename_ptr, strlen(tmp) + 1);
    if (*filename_ptr == NULL) {
      errormsg("out of memory!\n");
      exit(1);
    }
    strcpy(*filename_ptr, tmp);
  }
}

off_t filesize(char *filename) {
  struct stat s;

  if (stat(filename, &s) != 0) return -1;

  return s.st_size;
}

dev_t getdevice(char *filename) {
  struct stat s;

  if (stat(filename, &s) != 0) return 0;

  return s.st_dev;
}

ino_t getinode(char *filename) {
  struct stat s;
   
  if (stat(filename, &s) != 0) return 0;

  return s.st_ino;   
}

time_t getmtime(char *filename) {
  struct stat s;

  if (stat(filename, &s) != 0) return 0;

  return s.st_mtime;
}

wchar_t *wcsrstr(wchar_t *haystack, wchar_t *needle)
{
  wchar_t *found = 0;
  wchar_t *next = 0;

  found = wcsstr(haystack, needle);
  if (found)
  {
    do {
      next = wcsstr(found + 1, needle);
      if (next != 0)
        found = next;
    } while (next);

    return found;
  }

  return 0;
}

/* compare wide and multibyte strings */
int wcsmbcscmp(wchar_t *s1, char *s2)
{
  wchar_t *s2w;
  size_t size;
  int result;

  size = mbstowcs(0, s2, 0) + 1;

  s2w = (wchar_t*) malloc(size * sizeof(wchar_t));
  if (s2w == 0)
    return -1;

  mbstowcs(s2w, s2, size);

  result = wcscmp(s1, s2w);

  free(s2w);

  return result;
}

/* wide character needle is contained in multibyte haystack */
int wcsinmbcs(char *haystack, wchar_t *needle)
{
  wchar_t *haystackw;
  size_t size;
  int result;

  size = mbstowcs(0, haystack, 0) + 1;

  haystackw = (wchar_t*) malloc(size * sizeof(wchar_t));
  if (haystackw == 0)
    return -1;

  mbstowcs(haystackw, haystack, size);

  if (wcsstr(haystackw, needle) == 0)
    result = 0;
  else
    result = 1;

  free(haystackw);

  return result;
}

/* wide character needle at beginning of multibyte haystack */
int wcsbeginmbcs(char *haystack, wchar_t *needle)
{
  wchar_t *haystackw;
  size_t size;
  int result;

  size = mbstowcs(0, haystack, 0) + 1;

  haystackw = (wchar_t*) malloc(size * sizeof(wchar_t));
  if (haystackw == 0)
    return -1;

  mbstowcs(haystackw, haystack, size);

  if (wcsncmp(haystackw, needle, wcslen(needle)) == 0)
    result = 1;
  else
    result = 0;

  free(haystackw);

  return result;
}

/* wide character needle at end of multibyte haystack */
int wcsendsmbcs(char *haystack, wchar_t *needle)
{
  wchar_t *haystackw;
  size_t size;
  int result;

  size = mbstowcs(0, haystack, 0) + 1;

  haystackw = (wchar_t*) malloc(size * sizeof(wchar_t));
  if (haystackw == 0)
    return -1;

  mbstowcs(haystackw, haystack, size);

  if (wcsrstr(haystackw, needle) != 0 && wcscmp(wcsrstr(haystackw, needle), needle) == 0)
    result = 1;
  else
    result = 0;

  free(haystackw);

  return result;
}

char **cloneargs(int argc, char **argv)
{
  int x;
  char **args;

  args = (char **) malloc(sizeof(char*) * argc);
  if (args == NULL) {
    errormsg("out of memory!\n");
    exit(1);
  }

  for (x = 0; x < argc; x++) {
    args[x] = (char*) malloc(strlen(argv[x]) + 1);
    if (args[x] == NULL) {
      free(args);
      errormsg("out of memory!\n");
      exit(1);
    }

    strcpy(args[x], argv[x]);
  }

  return args;
}

int findarg(char *arg, int start, int argc, char **argv)
{
  int x;
  
  for (x = start; x < argc; x++)
    if (strcmp(argv[x], arg) == 0) 
      return x;

  return x;
}

/* Find the first non-option argument after specified option. */
int nonoptafter(char *option, int argc, char **oldargv, 
		      char **newargv, int optind) 
{
  int x;
  int targetind;
  int testind;
  int startat = 1;

  targetind = findarg(option, 1, argc, oldargv);
    
  for (x = optind; x < argc; x++) {
    testind = findarg(newargv[x], startat, argc, oldargv);
    if (testind > targetind) return x;
    else startat = testind;
  }

  return x;
}

int grokdir(char *dir, file_t **filelistp)
{
  DIR *cd;
  file_t *newfile;
  struct dirent *dirinfo;
  int lastchar;
  int filecount = 0;
  struct stat info;
  struct stat linfo;
  static int progress = 0;
  static char indicator[] = "-\\|/";

  cd = opendir(dir);

  if (!cd) {
    errormsg("could not chdir to %s\n", dir);
    return 0;
  }

  while ((dirinfo = readdir(cd)) != NULL) {
    if (strcmp(dirinfo->d_name, ".") && strcmp(dirinfo->d_name, "..")) {
      if (!ISFLAG(flags, F_HIDEPROGRESS)) {
	fprintf(stderr, "\rBuilding file list %c ", indicator[progress]);
	progress = (progress + 1) % 4;
      }

      newfile = (file_t*) malloc(sizeof(file_t));

      if (!newfile) {
	errormsg("out of memory!\n");
	closedir(cd);
	exit(1);
      } else newfile->next = *filelistp;

      newfile->device = 0;
      newfile->inode = 0;
      newfile->crcsignature = NULL;
      newfile->crcpartial = NULL;
      newfile->duplicates = NULL;
      newfile->hasdupes = 0;

      newfile->d_name = (char*)malloc(strlen(dir)+strlen(dirinfo->d_name)+2);

      if (!newfile->d_name) {
	errormsg("out of memory!\n");
	free(newfile);
	closedir(cd);
	exit(1);
      }

      strcpy(newfile->d_name, dir);
      lastchar = strlen(dir) - 1;
      if (lastchar >= 0 && dir[lastchar] != '/')
	strcat(newfile->d_name, "/");
      strcat(newfile->d_name, dirinfo->d_name);
      
      if (filesize(newfile->d_name) == 0 && ISFLAG(flags, F_EXCLUDEEMPTY)) {
	free(newfile->d_name);
	free(newfile);
	continue;
      }

      if (stat(newfile->d_name, &info) == -1) {
	free(newfile->d_name);
	free(newfile);
	continue;
      }

      if (lstat(newfile->d_name, &linfo) == -1) {
	free(newfile->d_name);
	free(newfile);
	continue;
      }

      if (S_ISDIR(info.st_mode)) {
	if (ISFLAG(flags, F_RECURSE) && (ISFLAG(flags, F_FOLLOWLINKS) || !S_ISLNK(linfo.st_mode)))
	  filecount += grokdir(newfile->d_name, filelistp);
	free(newfile->d_name);
	free(newfile);
      } else {
	if (S_ISREG(linfo.st_mode) || (S_ISLNK(linfo.st_mode) && ISFLAG(flags, F_FOLLOWLINKS))) {
	  *filelistp = newfile;
	  filecount++;
	} else {
	  free(newfile->d_name);
	  free(newfile);
	}
      }
    }
  }

  closedir(cd);

  return filecount;
}

#ifndef EXTERNAL_MD5

/* If EXTERNAL_MD5 is not defined, use L. Peter Deutsch's MD5 library. 
 */
char *getcrcsignatureuntil(char *filename, off_t max_read)
{
  int x;
  off_t fsize;
  off_t toread;
  md5_state_t state;
  md5_byte_t digest[16];  
  static md5_byte_t chunk[CHUNK_SIZE];
  static char signature[16*2 + 1]; 
  char *sigp;
  FILE *file;
   
  md5_init(&state);

 
  fsize = filesize(filename);
  
  if (max_read != 0 && fsize > max_read)
    fsize = max_read;

  file = fopen(filename, "rb");
  if (file == NULL) {
    errormsg("error opening file %s\n", filename);
    return NULL;
  }
 
  while (fsize > 0) {
    toread = (fsize % CHUNK_SIZE) ? (fsize % CHUNK_SIZE) : CHUNK_SIZE;
    if (fread(chunk, toread, 1, file) != 1) {
      errormsg("error reading from file %s\n", filename);
      fclose(file);
      return NULL;
    }
    md5_append(&state, chunk, toread);
    fsize -= toread;
  }

  md5_finish(&state, digest);

  sigp = signature;

  for (x = 0; x < 16; x++) {
    sprintf(sigp, "%02x", digest[x]);
    sigp = strchr(sigp, '\0');
  }

  fclose(file);

  return signature;
}

char *getcrcsignature(char *filename)
{
  return getcrcsignatureuntil(filename, 0);
}

char *getcrcpartialsignature(char *filename)
{
  return getcrcsignatureuntil(filename, PARTIAL_MD5_SIZE);
}

#endif /* [#ifndef EXTERNAL_MD5] */

#ifdef EXTERNAL_MD5

/* If EXTERNAL_MD5 is defined, use md5sum program to calculate signatures.
 */
char *getcrcsignature(char *filename)
{
  static char signature[256];
  char *command;
  char *separator;
  FILE *result;

  command = (char*) malloc(strlen(filename)+strlen(EXTERNAL_MD5)+2);
  if (command == NULL) {
    errormsg("out of memory\n");
    exit(1);
  }

  sprintf(command, "%s %s", EXTERNAL_MD5, filename);

  result = popen(command, "r");
  if (result == NULL) {
    errormsg("error invoking %s\n", EXTERNAL_MD5);
    exit(1);
  }
 
  free(command);

  if (fgets(signature, 256, result) == NULL) {
    errormsg("error generating signature for %s\n", filename);
    return NULL;
  }    
  separator = strchr(signature, ' ');
  if (separator) *separator = '\0';

  pclose(result);

  return signature;
}

#endif /* [#ifdef EXTERNAL_MD5] */

void purgetree(filetree_t *checktree)
{
  if (checktree->left != NULL) purgetree(checktree->left);
    
  if (checktree->right != NULL) purgetree(checktree->right);
    
  free(checktree);
}

void getfilestats(file_t *file)
{
  file->size = filesize(file->d_name);
  file->inode = getinode(file->d_name);
  file->device = getdevice(file->d_name);
  file->mtime = getmtime(file->d_name);
}

int registerfile(filetree_t **branch, file_t *file)
{
  getfilestats(file);

  *branch = (filetree_t*) malloc(sizeof(filetree_t));
  if (*branch == NULL) {
    errormsg("out of memory!\n");
    exit(1);
  }
  
  (*branch)->file = file;
  (*branch)->left = NULL;
  (*branch)->right = NULL;

  return 1;
}

file_t **checkmatch(filetree_t **root, filetree_t *checktree, file_t *file)
{
  int cmpresult;
  char *crcsignature;
  off_t fsize;

  /* If device and inode fields are equal one of the files is a 
     hard link to the other or the files have been listed twice 
     unintentionally. We don't want to flag these files as
     duplicates unless the user specifies otherwise.
  */    

  if (!ISFLAG(flags, F_CONSIDERHARDLINKS) && (getinode(file->d_name) == 
      checktree->file->inode) && (getdevice(file->d_name) ==
      checktree->file->device)) return NULL; 

  fsize = filesize(file->d_name);
  
  if (fsize < checktree->file->size) 
    cmpresult = -1;
  else 
    if (fsize > checktree->file->size) cmpresult = 1;
  else {
    if (checktree->file->crcpartial == NULL) {
      crcsignature = getcrcpartialsignature(checktree->file->d_name);
      if (crcsignature == NULL) return NULL;

      checktree->file->crcpartial = (char*) malloc(strlen(crcsignature)+1);
      if (checktree->file->crcpartial == NULL) {
	errormsg("out of memory\n");
	exit(1);
      }
      strcpy(checktree->file->crcpartial, crcsignature);
    }

    if (file->crcpartial == NULL) {
      crcsignature = getcrcpartialsignature(file->d_name);
      if (crcsignature == NULL) return NULL;

      file->crcpartial = (char*) malloc(strlen(crcsignature)+1);
      if (file->crcpartial == NULL) {
	errormsg("out of memory\n");
	exit(1);
      }
      strcpy(file->crcpartial, crcsignature);
    }

    cmpresult = strcmp(file->crcpartial, checktree->file->crcpartial);
    /*if (cmpresult != 0) errormsg("    on %s vs %s\n", file->d_name, checktree->file->d_name);*/

    if (cmpresult == 0) {
      if (checktree->file->crcsignature == NULL) {
	crcsignature = getcrcsignature(checktree->file->d_name);
	if (crcsignature == NULL) return NULL;

	checktree->file->crcsignature = (char*) malloc(strlen(crcsignature)+1);
	if (checktree->file->crcsignature == NULL) {
	  errormsg("out of memory\n");
	  exit(1);
	}
	strcpy(checktree->file->crcsignature, crcsignature);
      }

      if (file->crcsignature == NULL) {
	crcsignature = getcrcsignature(file->d_name);
	if (crcsignature == NULL) return NULL;

	file->crcsignature = (char*) malloc(strlen(crcsignature)+1);
	if (file->crcsignature == NULL) {
	  errormsg("out of memory\n");
	  exit(1);
	}
	strcpy(file->crcsignature, crcsignature);
      }

      cmpresult = strcmp(file->crcsignature, checktree->file->crcsignature);
      /*if (cmpresult != 0) errormsg("P   on %s vs %s\n", 
          file->d_name, checktree->file->d_name);
      else errormsg("P F on %s vs %s\n", file->d_name,
          checktree->file->d_name);
      printf("%s matches %s\n", file->d_name, checktree->file->d_name);*/
    }
  }

  if (cmpresult < 0) {
    if (checktree->left != NULL) {
      return checkmatch(root, checktree->left, file);
    } else {
      registerfile(&(checktree->left), file);
      return NULL;
    }
  } else if (cmpresult > 0) {
    if (checktree->right != NULL) {
      return checkmatch(root, checktree->right, file);
    } else {
      registerfile(&(checktree->right), file);
      return NULL;
    }
  } else 
  {
    getfilestats(file);
    return &checktree->file;
  }
}

/* Do a bit-for-bit comparison in case two different files produce the 
   same signature. Unlikely, but better safe than sorry. */

int confirmmatch(FILE *file1, FILE *file2)
{
  unsigned char c1 = 0;
  unsigned char c2 = 0;
  size_t r1;
  size_t r2;
  
  fseek(file1, 0, SEEK_SET);
  fseek(file2, 0, SEEK_SET);

  do {
    r1 = fread(&c1, sizeof(c1), 1, file1);
    r2 = fread(&c2, sizeof(c2), 1, file2);

    if (c1 != c2) return 0; /* file contents are different */
  } while (r1 && r2);
  
  if (r1 != r2) return 0; /* file lengths are different */

  return 1;
}

void summarizematches(file_t *files)
{
  int numsets = 0;
  double numbytes = 0.0;
  int numfiles = 0;
  file_t *tmpfile;

  while (files != NULL)
  {
    if (files->hasdupes)
    {
      numsets++;

      tmpfile = files->duplicates;
      while (tmpfile != NULL)
      {
	numfiles++;
	numbytes += files->size;
	tmpfile = tmpfile->duplicates;
      }
    }

    files = files->next;
  }

  if (numsets == 0)
    printf("No duplicates found.\n\n");
  else
  {
    if (numbytes < 1024.0)
      printf("%d duplicate files (in %d sets), occupying %.0f bytes.\n\n", numfiles, numsets, numbytes);
    else if (numbytes <= (1000.0 * 1000.0))
      printf("%d duplicate files (in %d sets), occupying %.1f kylobytes\n\n", numfiles, numsets, numbytes / 1000.0);
    else
      printf("%d duplicate files (in %d sets), occupying %.1f megabytes\n\n", numfiles, numsets, numbytes / (1000.0 * 1000.0));
 
  }
}

void printmatches(file_t *files)
{
  file_t *tmpfile;

  while (files != NULL) {
    if (files->hasdupes) {
      if (!ISFLAG(flags, F_OMITFIRST)) {
	if (ISFLAG(flags, F_SHOWSIZE)) printf("%lld byte%seach:\n", files->size,
	 (files->size != 1) ? "s " : " ");
	if (ISFLAG(flags, F_DSAMELINE)) escapefilename("\\ ", &files->d_name);
	printf("%s%c", files->d_name, ISFLAG(flags, F_DSAMELINE)?' ':'\n');
      }
      tmpfile = files->duplicates;
      while (tmpfile != NULL) {
	if (ISFLAG(flags, F_DSAMELINE)) escapefilename("\\ ", &tmpfile->d_name);
	printf("%s%c", tmpfile->d_name, ISFLAG(flags, F_DSAMELINE)?' ':'\n');
	tmpfile = tmpfile->duplicates;
      }
      printf("\n");

    }
      
    files = files->next;
  }
}

/*
#define REVISE_APPEND "_tmp"
char *revisefilename(char *path, int seq)
{
  int digits;
  char *newpath;
  char *scratch;
  char *dot;

  digits = numdigits(seq);
  newpath = malloc(strlen(path) + strlen(REVISE_APPEND) + digits + 1);
  if (!newpath) return newpath;

  scratch = malloc(strlen(path) + 1);
  if (!scratch) return newpath;

  strcpy(scratch, path);
  dot = strrchr(scratch, '.');
  if (dot) 
  {
    *dot = 0;
    sprintf(newpath, "%s%s%d.%s", scratch, REVISE_APPEND, seq, dot + 1);
  }

  else
  {
    sprintf(newpath, "%s%s%d", path, REVISE_APPEND, seq);
  }

  free(scratch);

  return newpath;
} */

int relink(char *oldfile, char *newfile)
{
  dev_t od;
  dev_t nd;
  ino_t oi;
  ino_t ni;

  od = getdevice(oldfile);
  oi = getinode(oldfile);

  if (link(oldfile, newfile) != 0)
    return 0;

  /* make sure we're working with the right file (the one we created) */
  nd = getdevice(newfile);
  ni = getinode(newfile);

  if (nd != od || oi != ni)
    return 0; /* file is not what we expected */

  return 1;
}

void deletefiles(file_t *files, int prompt, FILE *tty)
{
  int counter;
  int groups = 0;
  int curgroup = 0;
  file_t *tmpfile;
  file_t *curfile;
  file_t **dupelist;
  int *preserve;
  char *preservestr;
  char *token;
  char *tstr;
  int number;
  int sum;
  int max = 0;
  int x;
  int i;

  curfile = files;
  
  while (curfile) {
    if (curfile->hasdupes) {
      counter = 1;
      groups++;

      tmpfile = curfile->duplicates;
      while (tmpfile) {
	counter++;
	tmpfile = tmpfile->duplicates;
      }
      
      if (counter > max) max = counter;
    }
    
    curfile = curfile->next;
  }

  max++;

  dupelist = (file_t**) malloc(sizeof(file_t*) * max);
  preserve = (int*) malloc(sizeof(int) * max);
  preservestr = (char*) malloc(INPUT_SIZE);

  if (!dupelist || !preserve || !preservestr) {
    errormsg("out of memory\n");
    exit(1);
  }

  while (files) {
    if (files->hasdupes) {
      curgroup++;
      counter = 1;
      dupelist[counter] = files;

      if (prompt) printf("[%d] %s\n", counter, files->d_name);

      tmpfile = files->duplicates;

      while (tmpfile) {
	dupelist[++counter] = tmpfile;
	if (prompt) printf("[%d] %s\n", counter, tmpfile->d_name);
	tmpfile = tmpfile->duplicates;
      }

      if (prompt) printf("\n");

      if (!prompt) /* preserve only the first file */
      {
         preserve[1] = 1;
	 for (x = 2; x <= counter; x++) preserve[x] = 0;
      }

      else /* prompt for files to preserve */

      do {
	printf("Set %d of %d, preserve files [1 - %d, all]", 
          curgroup, groups, counter);
	if (ISFLAG(flags, F_SHOWSIZE)) printf(" (%lld byte%seach)", files->size,
	  (files->size != 1) ? "s " : " ");
	printf(": ");
	fflush(stdout);

	if (!fgets(preservestr, INPUT_SIZE, tty))
	  preservestr[0] = '\n'; /* treat fgets() failure as if nothing was entered */

	i = strlen(preservestr) - 1;

	while (preservestr[i]!='\n'){ /* tail of buffer must be a newline */
	  tstr = (char*)
	    realloc(preservestr, strlen(preservestr) + 1 + INPUT_SIZE);
	  if (!tstr) { /* couldn't allocate memory, treat as fatal */
	    errormsg("out of memory!\n");
	    exit(1);
	  }

	  preservestr = tstr;
	  if (!fgets(preservestr + i + 1, INPUT_SIZE, tty))
	  {
	    preservestr[0] = '\n'; /* treat fgets() failure as if nothing was entered */
	    break;
	  }
	  i = strlen(preservestr)-1;
	}

	for (x = 1; x <= counter; x++) preserve[x] = 0;
	
	token = strtok(preservestr, " ,\n");
	
	while (token != NULL) {
	  if (strcasecmp(token, "all") == 0)
	    for (x = 0; x <= counter; x++) preserve[x] = 1;
	  
	  number = 0;
	  sscanf(token, "%d", &number);
	  if (number > 0 && number <= counter) preserve[number] = 1;
	  
	  token = strtok(NULL, " ,\n");
	}
      
	for (sum = 0, x = 1; x <= counter; x++) sum += preserve[x];
      } while (sum < 1); /* make sure we've preserved at least one file */

      printf("\n");

      for (x = 1; x <= counter; x++) { 
	if (preserve[x])
	  printf("   [+] %s\n", dupelist[x]->d_name);
	else {
	  if (remove(dupelist[x]->d_name) == 0) {
	    printf("   [-] %s\n", dupelist[x]->d_name);
	  } else {
	    printf("   [!] %s ", dupelist[x]->d_name);
	    printf("-- unable to delete file!\n");
	  }
	}
      }
      printf("\n");
    }
    
    files = files->next;
  }

  free(dupelist);
  free(preserve);
  free(preservestr);
}

int sort_pairs_by_arrival(file_t *f1, file_t *f2)
{
  if (f2->duplicates != 0)
    return 1;

  return -1;
}

int sort_pairs_by_mtime(file_t *f1, file_t *f2)
{
  if (f1->mtime < f2->mtime)
    return -1;
  else if (f1->mtime > f2->mtime)
    return 1;

  return 0;
}

int get_num_digits(int value)
{
  int digits = 0;

  if (value < 0)
    value = -value;

  do {
    value /= 10;
    ++digits;
  } while (value > 0);

  return digits;
}

void print_spaces(WINDOW *window, int spaces)
{
  int x;

  for (x = 0; x < spaces; ++x)
    waddch(window, L' ');
}

void print_right_justified_int(WINDOW *window, int number, int width)
{
  int length;

  length = get_num_digits(number);
  if (number < 0)
    ++length;

  if (length < width)
    print_spaces(window, width - length);

  wprintw(window, "%d", number);
}

void putline(WINDOW *window, const char *str, const int line, const int columns, const int compensate_indent)
{
  wchar_t *dest = 0;
  int inputlength;
  int linestart;
  int linelength;
  int linewidth;
  int first_line_columns;
  int l;

  inputlength = mbstowcs(0, str, 0);
  if (inputlength == 0)
    return;

  dest = (wchar_t *) malloc((inputlength + 1) * sizeof(wchar_t));
  if (dest == NULL)
  {
    endwin();
    errormsg("out of memory\n");
    exit(1);
  }

  mbstowcs(dest, str, inputlength);
  dest[inputlength] = L'\0';

  first_line_columns = columns - compensate_indent;

  linestart = 0;

  if (line > 0)
  {
    linewidth = wcwidth(dest[linestart]);

    while (linestart + 1 < inputlength && linewidth + wcwidth(dest[linestart + 1]) <= first_line_columns)
      linewidth += wcwidth(dest[++linestart]);

    if (++linestart == inputlength)
      return;

    for (l = 1; l < line; ++l)
    {
      linewidth = wcwidth(dest[linestart]);

      while (linestart + 1 < inputlength && linewidth + wcwidth(dest[linestart + 1]) <= columns)
        linewidth += wcwidth(dest[++linestart]);

      if (++linestart == inputlength)
        return;
    }
  }

  linewidth = wcwidth(dest[linestart]);
  linelength = 1;

  if (line == 0)
  {
    while (linestart + linelength < inputlength && linewidth + wcwidth(dest[linestart + linelength]) <= first_line_columns)
    {
      linewidth += wcwidth(dest[linestart + linelength]);
      ++linelength;
    }
  }
  else
  {
    while (linestart + linelength < inputlength && linewidth + wcwidth(dest[linestart + linelength]) <= columns)
    {
      linewidth += wcwidth(dest[linestart + linelength]);
      ++linelength;
    }    
  }

  waddnwstr(window, dest + linestart, linelength);

  free(dest);
}

struct status_text
{
  wchar_t *left;
  wchar_t *right;
  size_t width;
};

struct status_text *status_text_alloc(struct status_text *status, size_t width)
{
  struct status_text *result;
  wchar_t *newleft;
  wchar_t *newright;

  if (status == 0)
  {
    result = (struct status_text*) malloc(sizeof(struct status_text));
    if (result == 0)
      return 0;

    result->left = (wchar_t*) malloc((width+1) * sizeof(wchar_t));
    if (result->left == 0)
    {
      free(result);
      return 0;
    }

    result->right = (wchar_t*) malloc((width+1) * sizeof(wchar_t));
    if (result->right == 0)
    {
      free(result->left);
      free(result);
      return 0;
    }

    result->left[0] = '\0';
    result->right[0] = '\0';

    result->width = width;
  }
  else
  {
    if (status->width >= width)
      return status;

    newleft = (wchar_t*) realloc(status->left, (width+1) * sizeof(wchar_t));
    if (newleft == 0)
      return 0;

    newright = (wchar_t*) realloc(status->right, (width+1) * sizeof(wchar_t));
    if (newright == 0)
    {
      free(newleft);
      return 0;
    }

    result = status;
    result->left = newleft;
    result->right = newright;
    result->width = width;
  }

  return result;
}

void format_status_left(struct status_text *status, wchar_t *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vswprintf(status->left, status->width + 1, format, ap);
  va_end(ap);
}

void format_status_right(struct status_text *status, wchar_t *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vswprintf(status->right, status->width + 1, format, ap);
  va_end(ap);
}

void print_status(WINDOW *statuswin, struct status_text *status)
{
  wchar_t *text;
  size_t cols;
  size_t x;
  size_t l;

  cols = getmaxx(statuswin);

  text = (wchar_t*)malloc((cols + 1) * sizeof(wchar_t));

  l = wcslen(status->left);
  for (x = 0; x < l && x < cols; ++x)
    text[x] = status->left[x];
  for (x = l; x < cols; ++x)
    text[x] = L' ';

  l = wcslen(status->right);
  for (x = cols; x >= 1 && l >= 1; --x, --l)
    text[x - 1] = status->right[l - 1];

  text[cols] = L'\0';

  wattron(statuswin, A_REVERSE);
  mvwaddnwstr(statuswin, 1, 0, text, wcslen(text));

  free(text);
}

void print_prompt(WINDOW *statuswin, wchar_t *prompt, ...)
{
  wchar_t *text;
  va_list ap;
  size_t cols;

  cols = getmaxx(statuswin);

  text = (wchar_t*)malloc((cols + 1) * sizeof(wchar_t));

  va_start(ap, prompt);
  vswprintf(text, cols, prompt, ap);
  va_end(ap);

  wattroff(statuswin, A_REVERSE);
  wmove(statuswin, 0, 0);
  wclrtoeol(statuswin);

  wattron(statuswin, A_BOLD);
  mvwaddnwstr(statuswin, 0, 0, text, wcslen(text));
  wattroff(statuswin, A_BOLD);

  free(text);
}

#define GET_COMMAND_OK 1
#define GET_COMMAND_CANCELED 2
#define GET_COMMAND_ERROR_OUT_OF_MEMORY 3
#define GET_COMMAND_RESIZE_REQUESTED 4
#define GET_COMMAND_KEY_NPAGE 5
#define GET_COMMAND_KEY_PPAGE 6
#define GET_COMMAND_KEY_SF 7
#define GET_COMMAND_KEY_SR 8

int get_command_text(wchar_t **commandbuffer, size_t *commandbuffersize, WINDOW *statuswin, int cancel_on_erase, int append)
{
  int docommandinput;
  int keyresult;
  wint_t wch;
  size_t length;
  size_t newsize;
  wchar_t *realloccommandbuffer;
  int cursor_x;
  int cursor_y;

  getyx(statuswin, cursor_y, cursor_x);

  if (*commandbuffer == 0)
  {
    *commandbuffersize = 80;
    *commandbuffer = malloc(*commandbuffersize * sizeof(wchar_t));
    if (*commandbuffer == 0)
      return GET_COMMAND_ERROR_OUT_OF_MEMORY;
  }

  if (!append)
  {
    (*commandbuffer)[0] = L'\0';
  }
  else
  {
    wprintw(statuswin, "%ls", *commandbuffer);

    wnoutrefresh(statuswin);
    doupdate();
  }

  docommandinput = 1;
  do
  {
    do
    {
      keyresult = wget_wch(statuswin, &wch);

      if (got_sigint)
      {
        got_sigint = 0;

        (*commandbuffer)[0] = '\0';

        return GET_COMMAND_CANCELED;
      }
    } while (keyresult == ERR);

    if (keyresult == OK)
    {
      switch (wch)
      {
        case KEY_ESCAPE:
          (*commandbuffer)[0] = '\0';

          return GET_COMMAND_CANCELED;

        case '\n':
          docommandinput = 0;
          continue;

        default:
          length = wcslen(*commandbuffer);

          if (length + 1 >= *commandbuffersize)
          {
            newsize = *commandbuffersize * 2;

            realloccommandbuffer = (wchar_t*)realloc(*commandbuffer, newsize * sizeof(wchar_t));
            if (realloccommandbuffer == 0)
              return GET_COMMAND_ERROR_OUT_OF_MEMORY;

            *commandbuffer = realloccommandbuffer;
            *commandbuffersize = newsize;
          }

          (*commandbuffer)[length] = wch;
          (*commandbuffer)[length+1] = L'\0';

          break;
      }
    }
    else if (keyresult == KEY_CODE_YES)
    {
      switch (wch)
      {
        case KEY_BACKSPACE:
          length = wcslen(*commandbuffer);

          if (cancel_on_erase && length <= 1)
          {
            (*commandbuffer)[0] = L'\0';

            return GET_COMMAND_CANCELED;
          }

          (*commandbuffer)[length-1] = L'\0';

          break;

        case KEY_NPAGE:
          return GET_COMMAND_KEY_NPAGE;

        case KEY_PPAGE:
          return GET_COMMAND_KEY_PPAGE;

        case KEY_SF:
          return GET_COMMAND_KEY_SF;

        case KEY_SR:
          return GET_COMMAND_KEY_SR;

        case KEY_RESIZE:
          return GET_COMMAND_RESIZE_REQUESTED;
      }
    }

    wmove(statuswin, cursor_y, cursor_x);
    wclrtoeol(statuswin);
    wmove(statuswin, cursor_y, cursor_x);

    wprintw(statuswin, "%ls", *commandbuffer);

    wnoutrefresh(statuswin);
    doupdate();
  } while (docommandinput);

  return GET_COMMAND_OK;
}

struct groupfile
{
  file_t *file;
  int action;
  int selected;
};

struct filegroup
{
  struct groupfile *files;
  size_t filecount;
  int startline;
  int endline;
  int selected;
};

enum linestyle
{
  linestyle_groupheader = 0,
  linestyle_groupheaderspacing,
  linestyle_filename,
  linestyle_groupfooterspacing
};

enum linestyle getlinestyle(struct filegroup *group, int line)
{
  if (line <= group->startline)
    return linestyle_groupheader;
  else if (line == group->startline + 1)
    return linestyle_groupheaderspacing;
  else if (line >= group->endline)
    return linestyle_groupfooterspacing;
  else
    return linestyle_filename;
}

#define FILENAME_INDENT_EXTRA 5
#define FILE_INDEX_MIN_WIDTH 3

int filerowcount(file_t *file, const int columns, int group_file_count)
{
  int lines;
  int line_remaining;
  int x = 0;
  size_t read;
  size_t filename_bytes;
  wchar_t ch;
  mbstate_t mbstate;
  int index_width;

  memset(&mbstate, 0, sizeof(mbstate));

  filename_bytes = strlen(file->d_name);

  index_width = get_num_digits(group_file_count);
  if (index_width < FILE_INDEX_MIN_WIDTH)
    index_width = FILE_INDEX_MIN_WIDTH;

  lines = (index_width + FILENAME_INDENT_EXTRA) / columns + 1;

  line_remaining = columns - (index_width + FILENAME_INDENT_EXTRA) % columns;

  while (x < filename_bytes)
  {
    read = mbrtowc(&ch, file->d_name + x, filename_bytes - x, &mbstate);
    if (read < 0)
      return lines;

    x += read;

    if (wcwidth(ch) <= line_remaining)
    {
      line_remaining -= wcwidth(ch);
    }
    else
    {
      line_remaining = columns - wcwidth(ch);
      ++lines;
    }
  }

  return lines;
}

int getgroupindex(struct filegroup *groups, int group_count, int group_hint, int line)
{
  int group = group_hint;

  while (group > 0 && line < groups[group].startline)
    --group;

  while (group < group_count && line > groups[group].endline)
    ++group;

  return group;
}

int getgroupfileindex(int *row, struct filegroup *group, int line, int columns)
{
  int l;
  int f = 0;
  int rowcount;

  l = group->startline + 2;

  while (f < group->filecount)
  {
    rowcount = filerowcount(group->files[f].file, columns, group->filecount);

    if (line <= l + rowcount - 1)
    {
      *row = line - l;
      return f;
    }

    l += rowcount;
    ++f;
  }

  return -1;
}

int getgroupfileline(struct filegroup *group, int fileindex, int columns)
{
  int l;
  int f = 0;
  int rowcount;

  l = group->startline + 2;

  while (f < fileindex && f < group->filecount)
  {
    rowcount = filerowcount(group->files[f].file, columns, group->filecount);
    l += rowcount;
    ++f;
  }

  return l;
}

void set_file_action(struct groupfile *file, int new_action, size_t *deletion_tally)
{
  switch (file->action)
  {
    case -1:
      if (new_action != -1)
        --*deletion_tally;
      break;

    default:
      if (new_action == -1)
        ++*deletion_tally;
      break;
  }

  file->action = new_action;
}

/* get command and arguments from user input */
void get_command_arguments(wchar_t **arguments, wchar_t *input)
{
  size_t l;
  size_t x;

  l = wcslen(input);

  for (x = 0; x < l; ++x)
    if (input[x] == L' ')
      break;

  if (input[x] == L' ')
    *arguments = input + x + 1;
  else
    *arguments = input + x;
}

/* select files containing string */
int cmd_select_containing(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status)
{
  int g;
  int f;
  int selectedgroupcount = 0;
  int selectedfilecount = 0;
  int groupselected;

  if (wcscmp(commandarguments, L"") != 0)
  {
    for (g = 0; g < groupcount; ++g)
    {
      groupselected = 0;

      for (f = 0; f < groups[g].filecount; ++f)
      {
        if (wcsinmbcs(groups[g].files[f].file->d_name, commandarguments))
        {
          groups[g].selected = 1;
          groups[g].files[f].selected = 1;

          groupselected = 1;
          ++selectedfilecount;
        }
      }

      if (groupselected)
        ++selectedgroupcount;
    }
  }

  format_status_left(status, L"Matched %d files in %d groups.", selectedfilecount, selectedgroupcount);

  return 1;
}

/* select files beginning with string */
int cmd_select_beginning(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status)
{
  int g;
  int f;
  int selectedgroupcount = 0;
  int selectedfilecount = 0;
  int groupselected;

  if (wcscmp(commandarguments, L"") != 0)
  {
    for (g = 0; g < groupcount; ++g)
    {
      groupselected = 0;

      for (f = 0; f < groups[g].filecount; ++f)
      {
        if (wcsbeginmbcs(groups[g].files[f].file->d_name, commandarguments))
        {
          groups[g].selected = 1;
          groups[g].files[f].selected = 1;

          groupselected = 1;
          ++selectedfilecount;
        }
      }

      if (groupselected)
        ++selectedgroupcount;
    }
  }

  format_status_left(status, L"Matched %d files in %d groups.", selectedfilecount, selectedgroupcount);

  return 1;
}

/* select files ending with string */
int cmd_select_ending(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status)
{
  int g;
  int f;
  int selectedgroupcount = 0;
  int selectedfilecount = 0;
  int groupselected;

  if (wcscmp(commandarguments, L"") != 0)
  {
    for (g = 0; g < groupcount; ++g)
    {
      groupselected = 0;

      for (f = 0; f < groups[g].filecount; ++f)
      {
        if (wcsendsmbcs(groups[g].files[f].file->d_name, commandarguments))
        {
          groups[g].selected = 1;
          groups[g].files[f].selected = 1;

          groupselected = 1;
          ++selectedfilecount;
        }
      }

      if (groupselected)
        ++selectedgroupcount;
    }
  }

  format_status_left(status, L"Matched %d files in %d groups.", selectedfilecount, selectedgroupcount);

  return 1;
}

/* select files matching string */
int cmd_select_matching(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status)
{
  int g;
  int f;
  int selectedgroupcount = 0;
  int selectedfilecount = 0;
  int groupselected;

  if (wcscmp(commandarguments, L"") != 0)
  {
    for (g = 0; g < groupcount; ++g)
    {
      groupselected = 0;

      for (f = 0; f < groups[g].filecount; ++f)
      {
        if (wcsmbcscmp(commandarguments, groups[g].files[f].file->d_name) == 0)
        {
          groups[g].selected = 1;
          groups[g].files[f].selected = 1;

          groupselected = 1;
          ++selectedfilecount;
        }
      }

      if (groupselected)
        ++selectedgroupcount;
    }
  }

  format_status_left(status, L"Matched %d files in %d groups.", selectedfilecount, selectedgroupcount);

  return 1;
}

/* select files matching pattern */
int cmd_select_regex(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status)
{
  size_t size;
  char *mbs;
  int errorcode;
  PCRE2_SIZE erroroffset;
  pcre2_code *code;
  pcre2_match_data *md;
  int matches;
  int g;
  int f;
  int selectedgroupcount = 0;
  int selectedfilecount = 0;
  int groupselected;

  size = wcstombs(0, commandarguments, 0) + 1;
  mbs = (char*) malloc(size * sizeof(char));
  if (mbs == 0)
    return -1;

  wcstombs(mbs, commandarguments, size);

  code = pcre2_compile((PCRE2_SPTR8)mbs, PCRE2_ZERO_TERMINATED, PCRE2_UTF | PCRE2_UCP, &errorcode, &erroroffset, 0);

  free(mbs);

  if (code == 0)
    return -1;

  pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);

  md = pcre2_match_data_create(1, 0);
  if (md == 0)
    return -1;

  for (g = 0; g < groupcount; ++g)
  {
    groupselected = 0;

    for (f = 0; f < groups[g].filecount; ++f)
    {
      matches = pcre2_match(code, (PCRE2_SPTR8)groups[g].files[f].file->d_name, PCRE2_ZERO_TERMINATED, 0, 0, md, 0);
      if (matches > 0)
      {
        groups[g].selected = 1;
        groups[g].files[f].selected = 1;

        groupselected = 1;
        ++selectedfilecount;
      }
    }

    if (groupselected)
      ++selectedgroupcount;
  }

  format_status_left(status, L"Matched %d files in %d groups.", selectedfilecount, selectedgroupcount);

  pcre2_code_free(code);

  return 1;
}

/* clear selections containing string */
int cmd_clear_selections_containing(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status)
{
  int g;
  int f;
  int matchedgroupcount = 0;
  int matchedfilecount = 0;
  int groupmatched;
  int filedeselected;
  int selectionsremaining;

  if (wcscmp(commandarguments, L"") != 0)
  {
    for (g = 0; g < groupcount; ++g)
    {
      groupmatched = 0;
      filedeselected = 0;
      selectionsremaining = 0;

      for (f = 0; f < groups[g].filecount; ++f)
      {
        if (wcsinmbcs(groups[g].files[f].file->d_name, commandarguments))
        {
          if (groups[g].files[f].selected)
          {
            groups[g].files[f].selected = 0;
            filedeselected = 1;
          }

          groupmatched = 1;
          ++matchedfilecount;
        }

        if (groups[g].files[f].selected)
          selectionsremaining = 1;
      }

      if (filedeselected && !selectionsremaining)
        groups[g].selected = 0;

      if (groupmatched)
        ++matchedgroupcount;
    }
  }

  format_status_left(status, L"Matched %d files in %d groups.", matchedfilecount, matchedgroupcount);

  return 1;
}

/* clear selections beginning with string */
int cmd_clear_selections_beginning(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status)
{
  int g;
  int f;
  int matchedgroupcount = 0;
  int matchedfilecount = 0;
  int groupmatched;
  int filedeselected;
  int selectionsremaining;

  if (wcscmp(commandarguments, L"") != 0)
  {
    for (g = 0; g < groupcount; ++g)
    {
      groupmatched = 0;
      filedeselected = 0;
      selectionsremaining = 0;

      for (f = 0; f < groups[g].filecount; ++f)
      {
        if (wcsbeginmbcs(groups[g].files[f].file->d_name, commandarguments))
        {
          if (groups[g].files[f].selected)
          {
            groups[g].files[f].selected = 0;
            filedeselected = 1;
          }

          groupmatched = 1;
          ++matchedfilecount;
        }

        if (groups[g].files[f].selected)
          selectionsremaining = 1;
      }

      if (filedeselected && !selectionsremaining)
        groups[g].selected = 0;

      if (groupmatched)
        ++matchedgroupcount;
    }
  }

  format_status_left(status, L"Matched %d files in %d groups.", matchedfilecount, matchedgroupcount);

  return 1;
}

/* clear selections ending with string */
int cmd_clear_selections_ending(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status)
{
  int g;
  int f;
  int matchedgroupcount = 0;
  int matchedfilecount = 0;
  int groupmatched;
  int filedeselected;
  int selectionsremaining;

  if (wcscmp(commandarguments, L"") != 0)
  {
    for (g = 0; g < groupcount; ++g)
    {
      groupmatched = 0;
      filedeselected = 0;
      selectionsremaining = 0;

      for (f = 0; f < groups[g].filecount; ++f)
      {
        if (wcsendsmbcs(groups[g].files[f].file->d_name, commandarguments))
        {
          if (groups[g].files[f].selected)
          {
            groups[g].files[f].selected = 0;
            filedeselected = 1;
          }

          groupmatched = 1;
          ++matchedfilecount;
        }

        if (groups[g].files[f].selected)
          selectionsremaining = 1;
      }

      if (filedeselected && !selectionsremaining)
        groups[g].selected = 0;

      if (groupmatched)
        ++matchedgroupcount;
    }
  }

  format_status_left(status, L"Matched %d files in %d groups.", matchedfilecount, matchedgroupcount);

  return 1;
}

/* clear selections matching string */
int cmd_clear_selections_matching(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status)
{
  int g;
  int f;
  int matchedgroupcount = 0;
  int matchedfilecount = 0;
  int groupmatched;
  int filedeselected;
  int selectionsremaining;

  if (wcscmp(commandarguments, L"") != 0)
  {
    for (g = 0; g < groupcount; ++g)
    {
      groupmatched = 0;
      filedeselected = 0;
      selectionsremaining = 0;

      for (f = 0; f < groups[g].filecount; ++f)
      {
        if (wcsmbcscmp(commandarguments, groups[g].files[f].file->d_name) == 0)
        {
          if (groups[g].files[f].selected)
          {
            groups[g].files[f].selected = 0;
            filedeselected = 1;
          }

          groupmatched = 1;
          ++matchedfilecount;
        }

        if (groups[g].files[f].selected)
          selectionsremaining = 1;
      }

      if (filedeselected && !selectionsremaining)
        groups[g].selected = 0;

      if (groupmatched)
        ++matchedgroupcount;
    }
  }

  format_status_left(status, L"Matched %d files in %d groups.", matchedfilecount, matchedgroupcount);

  return 1;
}

/* clear selection matching pattern */
int cmd_clear_selections_regex(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status)
{
  size_t size;
  char *mbs;
  int errorcode;
  PCRE2_SIZE erroroffset;
  pcre2_code *code;
  pcre2_match_data *md;
  int matches;
  int g;
  int f;
  int matchedgroupcount = 0;
  int matchedfilecount = 0;
  int groupmatched;
  int filedeselected;
  int selectionsremaining;

  size = wcstombs(0, commandarguments, 0) + 1;
  mbs = (char*) malloc(size * sizeof(char));
  if (mbs == 0)
    return -1;

  wcstombs(mbs, commandarguments, size);

  code = pcre2_compile((PCRE2_SPTR8)mbs, PCRE2_ZERO_TERMINATED, PCRE2_UTF | PCRE2_UCP, &errorcode, &erroroffset, 0);

  free(mbs);

  if (code == 0)
    return -1;

  pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);

  md = pcre2_match_data_create(1, 0);
  if (md == 0)
    return -1;

  for (g = 0; g < groupcount; ++g)
  {
    groupmatched = 0;
    filedeselected = 0;
    selectionsremaining = 0;

    for (f = 0; f < groups[g].filecount; ++f)
    {
      matches = pcre2_match(code, (PCRE2_SPTR8)groups[g].files[f].file->d_name, PCRE2_ZERO_TERMINATED, 0, 0, md, 0);
      if (matches > 0)
      {
        if (groups[g].files[f].selected)
        {
          groups[g].files[f].selected = 0;
          filedeselected = 1;
        }

        groupmatched = 1;
        ++matchedfilecount;
      }

      if (groups[g].files[f].selected)
        selectionsremaining = 1;
    }

    if (filedeselected && !selectionsremaining)
      groups[g].selected = 0;

    if (groupmatched)
      ++matchedgroupcount;
  }

  format_status_left(status, L"Matched %d files in %d groups.", matchedfilecount, matchedgroupcount);

  pcre2_code_free(code);

  return 1;
}

/* clear all selections and selected groups */
int cmd_clear_all_selections(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status)
{
  int g;
  int f;

  for (g = 0; g < groupcount; ++g)
  {
    for (f = 0; f < groups[g].filecount; ++f)
      groups[g].files[f].selected = 0;

    groups[g].selected = 0;
  }

  format_status_left(status, L"Cleared all selections.");

  return 1;
}

/* invert selections within selected groups */
int cmd_invert_group_selections(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status)
{
  int g;
  int f;
  int selectedcount = 0;
  int deselectedcount = 0;

  for (g = 0; g < groupcount; ++g)
  {
    if (groups[g].selected)
    {
      for (f = 0; f < groups[g].filecount; ++f)
      {
        groups[g].files[f].selected = !groups[g].files[f].selected;

        if (groups[g].files[f].selected)
          ++selectedcount;
        else
          ++deselectedcount;
      }
    }
  }

  format_status_left(status, L"Selected %d files. Deselected %d files.", selectedcount, deselectedcount);

  return 1;
}

/* mark selected files for preservation */
int cmd_keep_selected(struct filegroup *groups, int groupcount, wchar_t *commandarguments, size_t *deletiontally, struct status_text *status)
{
  int g;
  int f;
  int keepfilecount = 0;

  for (g = 0; g < groupcount; ++g)
  {
    for (f = 0; f < groups[g].filecount; ++f)
    {
      if (groups[g].files[f].selected)
      {
        set_file_action(&groups[g].files[f], 1, deletiontally);
        ++keepfilecount;
      }
    }
  }

  format_status_left(status, L"Marked %d files for preservation.", keepfilecount);

  return 1;
}

/* mark selected files for deletion */
int cmd_delete_selected(struct filegroup *groups, int groupcount, wchar_t *commandarguments, size_t *deletiontally, struct status_text *status)
{
  int g;
  int f;
  int deletefilecount = 0;

  for (g = 0; g < groupcount; ++g)
  {
    for (f = 0; f < groups[g].filecount; ++f)
    {
      if (groups[g].files[f].selected)
      {
        set_file_action(&groups[g].files[f], -1, deletiontally);
        ++deletefilecount;
      }
    }
  }

  format_status_left(status, L"Marked %d files for deletion.", deletefilecount);

  return 1;
}

/* mark selected files as unresolved */
int cmd_reset_selected(struct filegroup *groups, int groupcount, wchar_t *commandarguments, size_t *deletiontally, struct status_text *status)
{
  int g;
  int f;
  int resetfilecount = 0;

  for (g = 0; g < groupcount; ++g)
  {
    for (f = 0; f < groups[g].filecount; ++f)
    {
      if (groups[g].files[f].selected)
      {
        set_file_action(&groups[g].files[f], 0, deletiontally);
        ++resetfilecount;
      }
    }
  }

  format_status_left(status, L"Unmarked %d files.", resetfilecount);

  return 1;
}

#define FILE_LIST_OK 1
#define FILE_LIST_ERROR_INDEX_OUT_OF_RANGE -1
#define FILE_LIST_ERROR_LIST_CONTAINS_INVALID_INDEX -2
#define FILE_LIST_ERROR_UNKNOWN_COMMAND -3
#define FILE_LIST_ERROR_OUT_OF_MEMORY -4

int validate_file_list(struct filegroup *currentgroup, wchar_t *commandbuffer_in)
{
  wchar_t *commandbuffer;
  wchar_t *token;
  wchar_t *wcsptr;
  wchar_t *wcstolcheck;
  long int number;
  int parts = 0;
  int parse_error = 0;
  int out_of_bounds_error = 0;

  commandbuffer = malloc(sizeof(wchar_t) * (wcslen(commandbuffer_in)+1));
  if (commandbuffer == 0)
    return FILE_LIST_ERROR_OUT_OF_MEMORY;

  wcscpy(commandbuffer, commandbuffer_in);

  token = wcstok(commandbuffer, L",", &wcsptr);

  while (token != NULL)
  {
    ++parts;

    number = wcstol(token, &wcstolcheck, 10);
    if (wcstolcheck == token || *wcstolcheck != '\0')
      parse_error = 1;

    if (number > currentgroup->filecount || number < 1)
      out_of_bounds_error = 1;

    token = wcstok(NULL, L",", &wcsptr);
  }

  free(commandbuffer);

  if (parts == 1 && parse_error)
    return FILE_LIST_ERROR_UNKNOWN_COMMAND;
  else if (parse_error)
    return FILE_LIST_ERROR_LIST_CONTAINS_INVALID_INDEX;
  else if (out_of_bounds_error)
    return FILE_LIST_ERROR_INDEX_OUT_OF_RANGE;

  return FILE_LIST_OK;
}

/* command IDs */
#define COMMAND_RECOGNIZER_OUT_OF_MEMORY -2
#define COMMAND_AMBIGUOUS -1
#define COMMAND_UNDEFINED 0
#define COMMAND_SELECT_CONTAINING 1
#define COMMAND_SELECT_BEGINNING 2
#define COMMAND_SELECT_ENDING 3
#define COMMAND_SELECT_MATCHING 4
#define COMMAND_CLEAR_SELECTIONS_CONTAINING 5
#define COMMAND_CLEAR_SELECTIONS_BEGINNING 6
#define COMMAND_CLEAR_SELECTIONS_ENDING 7
#define COMMAND_CLEAR_SELECTIONS_MATCHING 8
#define COMMAND_CLEAR_ALL_SELECTIONS 9
#define COMMAND_INVERT_GROUP_SELECTIONS 10
#define COMMAND_KEEP_SELECTED 11
#define COMMAND_DELETE_SELECTED 12
#define COMMAND_RESET_SELECTED 13
#define COMMAND_RESET_GROUP 14
#define COMMAND_PRESERVE_ALL 15
#define COMMAND_EXIT 16
#define COMMAND_HELP 17
#define COMMAND_YES 18
#define COMMAND_NO 19
#define COMMAND_SELECT_REGEX 20
#define COMMAND_CLEAR_SELECTIONS_REGEX 21
#define COMMAND_GOTO_SET 22

/* command name to command ID mappings */
struct command_map {
  wchar_t *command_name;
  int command;
} command_list[] = {
  {L"sel", COMMAND_SELECT_CONTAINING},
  {L"selb", COMMAND_SELECT_BEGINNING},
  {L"sele", COMMAND_SELECT_ENDING},
  {L"selm", COMMAND_SELECT_MATCHING},
  {L"selr", COMMAND_SELECT_REGEX},
  {L"dsel", COMMAND_CLEAR_SELECTIONS_CONTAINING},
  {L"dselb", COMMAND_CLEAR_SELECTIONS_BEGINNING},
  {L"dsele", COMMAND_CLEAR_SELECTIONS_ENDING},
  {L"dselm", COMMAND_CLEAR_SELECTIONS_MATCHING},
  {L"dselr", COMMAND_CLEAR_SELECTIONS_REGEX},
  {L"cs", COMMAND_CLEAR_ALL_SELECTIONS},
  {L"igs", COMMAND_INVERT_GROUP_SELECTIONS},
  {L"ks", COMMAND_KEEP_SELECTED},
  {L"ds", COMMAND_DELETE_SELECTED},
  {L"rs", COMMAND_RESET_SELECTED},
  {L"rg", COMMAND_RESET_GROUP},
  {L"all", COMMAND_PRESERVE_ALL},
  {L"goto", COMMAND_GOTO_SET},
  {L"exit", COMMAND_EXIT},
  {L"quit", COMMAND_EXIT},
  {L"help", COMMAND_HELP},
  {0, COMMAND_UNDEFINED}
};

struct command_map confirmation_keyword_list[] = {
  {L"yes", COMMAND_YES},
  {L"no", COMMAND_NO},
  {0, COMMAND_UNDEFINED}
};

/* command identifier node structure */
struct command_identifier_node
{
  int command;
  wchar_t character;
  struct command_identifier_node **children;
  size_t num_children;
};

/* insert new node into command identifier tree */
int insert_command_identifier_command(struct command_identifier_node *tree, struct command_map *command, size_t ch)
{
  struct command_identifier_node *child;
  struct command_identifier_node **alloc_children;
  int returned_command;
  int c;

  /* find node for current character in command name */
  child = 0;
  for (c = 0; c < tree->num_children; ++c)
  {
    if (tree->children[c]->character == command->command_name[ch])
    {
      child = tree->children[c];
      break;
    }
  }

  /* if sought node does not exist, create it */
  if (child == 0)
  {
    child = (struct command_identifier_node*) malloc(sizeof(struct command_identifier_node));
    if (child == 0)
      return COMMAND_RECOGNIZER_OUT_OF_MEMORY;

    child->command = COMMAND_UNDEFINED;
    child->character = command->command_name[ch];
    child->children = 0;
    child->num_children = 0;

    alloc_children = realloc(tree->children, sizeof(struct command_identifier_node*) * tree->num_children + 1);
    if (alloc_children == 0)
      return COMMAND_RECOGNIZER_OUT_OF_MEMORY;

    tree->children = alloc_children;

    tree->children[tree->num_children++] = child;
  }

  /* if last character in command name, make child a leaf node */
  if (command->command_name[ch] == L'\0')
  {
    child->command = command->command;
    return child->command;
  }
  else /* grow the tree */
  {
    returned_command = insert_command_identifier_command(child, command, ch + 1);

    /* record whether the tree at this point leads to a single command (abbreviation) or many (ambiguous) */
    if (tree->command == COMMAND_UNDEFINED)
      tree->command = returned_command;
    else
      tree->command = COMMAND_AMBIGUOUS;
  }

  return tree->command;
}

/* compare two command identifier nodes by the characters they match */
int compare_command_identifier_nodes(const void *a, const void *b)
{
  const struct command_identifier_node *aa;
  const struct command_identifier_node *bb;

  aa = *(struct command_identifier_node**)a;
  bb = *(struct command_identifier_node**)b;

  if (aa->character > bb->character)
    return 1;
  else if (aa->character < bb->character)
    return -1;
  else
    return 0;
}

/* sort command identifier nodes in alphabetical order */
void sort_command_identifier_nodes(struct command_identifier_node *root)
{
  int c;

  if (root->num_children > 1)
    qsort(root->children, root->num_children, sizeof(struct command_identifier_node *), compare_command_identifier_nodes);

  for (c = 0; c < root->num_children; ++c)
    sort_command_identifier_nodes(root->children[c]);
}

/* build tree to identify command names through state transitions */
struct command_identifier_node *build_command_identifier_tree(struct command_map *commands)
{
  struct command_identifier_node *root;
  int c;

  root = (struct command_identifier_node*) malloc(sizeof(struct command_identifier_node));
  if (root == 0)
    return 0;

  root->command = COMMAND_UNDEFINED;
  root->character = L'\0';
  root->children = 0;
  root->num_children = 0;

  c = 0;
  while (commands[c].command_name != 0)
  {
    insert_command_identifier_command(root, commands + c, 0);
    ++c;
  }

  sort_command_identifier_nodes(root);

  return root;
}

/* free memory used by command identifier tree structure */
void free_command_identifier_tree(struct command_identifier_node *tree)
{
  int c;

  for (c = 0; c < tree->num_children; ++c)
    free_command_identifier_tree(tree->children[c]);

  free(tree);
}

/* find command identifier node matching given character */
struct command_identifier_node *find_command_identifier_node(struct command_identifier_node *root, wchar_t character)
{
  long min;
  long max;
  long mid;

  if (root->num_children == 0)
    return 0;

  min = 0;
  max = root->num_children - 1;

  do
  {
    mid = (min + max) / 2;

    if (character > root->children[mid]->character)
      min = mid + 1;
    else if (character < root->children[mid]->character)
      max = mid - 1;
    else
      return root->children[mid];
  } while (min <= max);

  return 0;
}

/* determine ID for given command string (possibly abbreviated), if found */
int identify_command(struct command_identifier_node *tree, wchar_t *command_buffer, size_t ch)
{
  struct command_identifier_node *found;

  if (command_buffer[ch] != L' ')
    found = find_command_identifier_node(tree, command_buffer[ch]);
  else
    found = find_command_identifier_node(tree, L'\0');

  if (found)
  {
    if (command_buffer[ch] == L'\0' || command_buffer[ch] == L' ')
      return found->command;
    else
      return identify_command(found, command_buffer, ch + 1);
  }
  else
  {
    if (command_buffer[ch] == L'\0' || command_buffer[ch] == L' ')
      return tree->command;
    else
      return COMMAND_UNDEFINED;
  }
}

void scroll_to_group(int *topline, int group, int tail, struct filegroup *groups, WINDOW *filewin)
{
  if (*topline < groups[group].startline)
  {
    if (groups[group].endline >= *topline + getmaxy(filewin))
    {
      if (groups[group].endline - groups[group].startline < getmaxy(filewin))
        *topline = groups[group].endline - getmaxy(filewin) + 1;
      else
        *topline = groups[group].startline;
    }
  }
  else
  {
    if (groups[group].endline - groups[group].startline < getmaxy(filewin) || !tail)
      *topline = groups[group].startline;
    else
      *topline = groups[group].endline - getmaxy(filewin);
  }
}

void move_to_next_group(int *topline, int *cursorgroup, int *cursorfile, struct filegroup *groups, WINDOW *filewin)
{
  *cursorgroup += 1;

  *cursorfile = 0;

  scroll_to_group(topline, *cursorgroup, 0, groups, filewin);
}

int move_to_next_selected_group(int *topline, int *cursorgroup, int *cursorfile, struct filegroup *groups, int totalgroups, WINDOW *filewin)
{
  size_t g;

  for (g = *cursorgroup + 1; g < totalgroups; ++g)
  {
    if (groups[g].selected)
    {
      *cursorgroup = g;
      *cursorfile = 0;

      scroll_to_group(topline, *cursorgroup, 0, groups, filewin);

      return 1;
    }
  }

  return 0;
}

void move_to_next_file(int *topline, int *cursorgroup, int *cursorfile, struct filegroup *groups, WINDOW *filewin)
{
  *cursorfile += 1;

  if (getgroupfileline(&groups[*cursorgroup], *cursorfile, COLS) >= *topline + getmaxy(filewin))
  {
      if (groups[*cursorgroup].endline - getgroupfileline(&groups[*cursorgroup], *cursorfile, COLS) < getmaxy(filewin))
        *topline = groups[*cursorgroup].endline - getmaxy(filewin) + 1;
      else
        *topline = getgroupfileline(&groups[*cursorgroup], *cursorfile, COLS);
  }
}

void move_to_previous_group(int *topline, int *cursorgroup, int *cursorfile, struct filegroup *groups, WINDOW *filewin)
{
  *cursorgroup -= 1;

  *cursorfile = groups[*cursorgroup].filecount - 1;

  scroll_to_group(topline, *cursorgroup, 1, groups, filewin);
}

int move_to_previous_selected_group(int *topline, int *cursorgroup, int *cursorfile, struct filegroup *groups, int totalgroups, WINDOW *filewin)
{
  size_t g;

  for (g = *cursorgroup; g > 0; --g)
  {
    if (groups[g - 1].selected)
    {
      *cursorgroup = g - 1;
      *cursorfile = 0;

      scroll_to_group(topline, *cursorgroup, 0, groups, filewin);

      return 1;
    }
  }

  return 0;
}

void move_to_previous_file(int *topline, int *cursorgroup, int *cursorfile, struct filegroup *groups, WINDOW *filewin)
{
  *cursorfile -= 1;

  if (getgroupfileline(&groups[*cursorgroup], *cursorfile, COLS) < *topline)
  {
      if (getgroupfileline(&groups[*cursorgroup], *cursorfile, COLS) - groups[*cursorgroup].startline < getmaxy(filewin))
        *topline -= getgroupfileline(&groups[*cursorgroup], *cursorfile, COLS) - groups[*cursorgroup].startline + 1;
      else
        *topline -= getmaxy(filewin);
  }
}

void sigint_handler(int signal)
{
  got_sigint = 1;
}

void deletefiles_ncurses(file_t *files)
{
  WINDOW *filewin;
  WINDOW *statuswin;
  file_t *curfile;
  file_t *dupefile;
  struct filegroup *groups;
  struct filegroup *reallocgroups;
  size_t groupfilecount;
  int topline = 0;
  int cursorgroup = 0;
  int cursorfile = 0;
  int cursor_x;
  int cursor_y;
  int groupfirstline = 0;
  int totallines = 0;
  int allocatedgroups = 0;
  int totalgroups = 0;
  size_t groupindex = 0;
  enum linestyle linestyle;
  int preservecount;
  int deletecount;
  int unresolvedcount;
  int totaldeleted;
  size_t globaldeletiontally = 0;
  double deletedbytes;
  int row;
  int x;
  int g;
  wint_t wch;
  int keyresult;
  int cy;
  int f;
  int to;
  wchar_t *commandbuffer;
  size_t commandbuffersize;
  wchar_t *commandarguments;
  struct command_identifier_node *commandidentifier;
  struct command_identifier_node *confirmationkeywordidentifier;
  int doprune;
  wchar_t *token;
  wchar_t *wcsptr;
  wchar_t *wcstolcheck;
  long int number;
  struct status_text *status;
  int dupesfound;
  int intresult;
  struct sigaction action;
  int adjusttopline;
  int toplineoffset = 0;
  int resumecommandinput = 0;
  int index_width;

  initscr();
  noecho();
  cbreak();
  halfdelay(5);

  filewin = newwin(LINES - 2, COLS, 0, 0);
  statuswin = newwin(2, COLS, LINES - 2, 0);

  scrollok(filewin, FALSE);

  wattron(statuswin, A_REVERSE);

  keypad(statuswin, 1);

  commandbuffersize = 80;
  commandbuffer = malloc(commandbuffersize * sizeof(wchar_t));
  if (commandbuffer == 0)
  {
    endwin();
    errormsg("out of memory\n");
    exit(1);
  }

  allocatedgroups = 1024;
  groups = malloc(sizeof(struct filegroup) * allocatedgroups);
  if (groups == 0)
  {
    free(commandbuffer);

    endwin();
    errormsg("out of memory\n");
    exit(1);
  }

  commandidentifier = build_command_identifier_tree(command_list);
  if (commandidentifier == 0)
  {
    free(groups);
    free(commandbuffer);

    endwin();
    errormsg("out of memory\n");
    exit(1);
  }

  confirmationkeywordidentifier = build_command_identifier_tree(confirmation_keyword_list);
  if (confirmationkeywordidentifier == 0)
  {
    free(groups);
    free(commandbuffer);
    free_command_identifier_tree(commandidentifier);

    endwin();
    errormsg("out of memory\n");
    exit(1);
  }

  action.sa_handler = sigint_handler;
  sigaction(SIGINT, &action, 0);

  curfile = files;
  while (curfile)
  {
    if (!curfile->hasdupes)
    {
      curfile = curfile->next;
      continue;
    }

    if (totalgroups + 1 > allocatedgroups)
    {
      allocatedgroups *= 2;

      reallocgroups = realloc(groups, sizeof(struct filegroup) * allocatedgroups);
      if (reallocgroups == 0)
      {
        for (g = 0; g < totalgroups; ++g)
          free(groups[g].files);

        free(groups);
        free(commandbuffer);
        free_command_identifier_tree(commandidentifier);
        free_command_identifier_tree(confirmationkeywordidentifier);

        endwin();
        errormsg("out of memory\n");
        exit(1);
      }

      groups = reallocgroups;
    }

    groups[totalgroups].startline = groupfirstline;
    groups[totalgroups].endline = groupfirstline + 2;
    groups[totalgroups].selected = 0;

    groupfilecount = 0;

    dupefile = curfile;
    do
    {
      ++groupfilecount;

      dupefile = dupefile->duplicates;
    } while(dupefile);

    dupefile = curfile;
    do
    {
      groups[totalgroups].endline += filerowcount(dupefile, COLS, groupfilecount);

      dupefile = dupefile->duplicates;
    } while (dupefile);

    groups[totalgroups].files = malloc(sizeof(struct groupfile) * groupfilecount);
    if (groups[totalgroups].files == 0)
    {
      for (g = 0; g < totalgroups; ++g)
        free(groups[g].files);

      free(groups);

      endwin();
      errormsg("out of memory\n");
      exit(1);
    }

    groupfilecount = 0;

    dupefile = curfile;
    do
    {
      groups[totalgroups].files[groupfilecount].file = dupefile;
      groups[totalgroups].files[groupfilecount].action = 0;
      groups[totalgroups].files[groupfilecount].selected = 0;
      ++groupfilecount;

      dupefile = dupefile->duplicates;
    } while (dupefile);

    groups[totalgroups].filecount = groupfilecount;

    groupfirstline = groups[totalgroups].endline + 1;

    ++totalgroups;

    curfile = curfile->next;
  }

  dupesfound = totalgroups > 0;

  status = status_text_alloc(0, COLS);
  format_status_left(status, L"Ready");

  doprune = 1;
  do
  {
    wmove(filewin, 0, 0);
    werase(filewin);

    totallines = groups[totalgroups-1].endline;

    for (x = topline; x < topline + getmaxy(filewin); ++x)
    {
      if (x >= totallines)
      {
        wclrtoeol(filewin);
        continue;
      }

      groupindex = getgroupindex(groups, totalgroups, groupindex, x);

      index_width = get_num_digits(groups[groupindex].filecount);

      if (index_width < FILE_INDEX_MIN_WIDTH)
        index_width = FILE_INDEX_MIN_WIDTH;

      linestyle = getlinestyle(groups + groupindex, x);
      
      if (linestyle == linestyle_groupheader)
      {
        wattron(filewin, A_UNDERLINE);
        if (groups[groupindex].selected)
          wattron(filewin, A_REVERSE);
        wprintw(filewin, "Set %d of %d:\n", groupindex + 1, totalgroups);
        if (groups[groupindex].selected)
          wattroff(filewin, A_REVERSE);
        wattroff(filewin, A_UNDERLINE);
      }
      else if (linestyle == linestyle_groupheaderspacing)
      {
        wprintw(filewin, "\n");
      }
      else if (linestyle == linestyle_filename)
      {
        f = getgroupfileindex(&row, groups + groupindex, x, COLS);

        if (cursorgroup != groupindex)
        {
          if (row == 0)
          {
            print_spaces(filewin, index_width);

            wprintw(filewin, " [%c] ", groups[groupindex].files[f].action > 0 ? '+' : groups[groupindex].files[f].action < 0 ? '-' : ' ');
          }

          cy = getcury(filewin);

          if (groups[groupindex].files[f].selected)
            wattron(filewin, A_REVERSE);
          putline(filewin, groups[groupindex].files[f].file->d_name, row, COLS, index_width + FILENAME_INDENT_EXTRA);
          if (groups[groupindex].files[f].selected)
            wattroff(filewin, A_REVERSE);

          wclrtoeol(filewin);
          wmove(filewin, cy+1, 0);
        }
        else
        {
          if (row == 0)
          {
            print_right_justified_int(filewin, f+1, index_width);
            wprintw(filewin, " ");

            if (cursorgroup == groupindex && cursorfile == f)
              wattron(filewin, A_REVERSE);
            wprintw(filewin, "[%c]", groups[groupindex].files[f].action > 0 ? '+' : groups[groupindex].files[f].action < 0 ? '-' : ' ');
            if (cursorgroup == groupindex && cursorfile == f)
              wattroff(filewin, A_REVERSE);
            wprintw(filewin, " ");
          }

          cy = getcury(filewin);

          if (groups[groupindex].files[f].selected)
            wattron(filewin, A_REVERSE);
          putline(filewin, groups[groupindex].files[f].file->d_name, row, COLS, index_width + FILENAME_INDENT_EXTRA);
          if (groups[groupindex].files[f].selected)
            wattroff(filewin, A_REVERSE);

          wclrtoeol(filewin);
          wmove(filewin, cy+1, 0);
        }
      }
      else if (linestyle == linestyle_groupfooterspacing)
      {
        wprintw(filewin, "\n");
      }
    }

    wnoutrefresh(filewin);

    if (totalgroups > 0)
      format_status_right(status, L"Set %d of %d", cursorgroup+1, totalgroups);
    else
      format_status_right(status, L"Finished");

    print_status(statuswin, status);

    if (totalgroups > 0)
      print_prompt(statuswin, L"[ Preserve files (1 - %d, all, help) ]:", groups[cursorgroup].filecount);
    else if (dupesfound)
      print_prompt(statuswin, L"[ No duplicates remaining (type 'exit' to exit program) ]:");
    else
      print_prompt(statuswin, L"[ No duplicates found (type 'exit' to exit program) ]:");

    wprintw(statuswin, " ");

    wnoutrefresh(statuswin);
    doupdate();

    /* wait for user input */
    if (!resumecommandinput)
    {
      do
      {
        keyresult = wget_wch(statuswin, &wch);

        if (got_sigint)
        {
          getyx(statuswin, cursor_y, cursor_x);

          format_status_left(status, L"Type 'exit' to exit fdupes.");
          print_status(statuswin, status);

          wattroff(statuswin, A_REVERSE);
          wmove(statuswin, cursor_y, cursor_x);

          got_sigint = 0;

          wnoutrefresh(statuswin);
          doupdate();
        }
      } while (keyresult == ERR);

      commandbuffer[0] = wch;
      commandbuffer[1] = '\0';
    }

    if (resumecommandinput || (keyresult == OK && ((wch != '\t' && wch != '\n' && wch != '?'))))
    {
      resumecommandinput = 0;

      switch (get_command_text(&commandbuffer, &commandbuffersize, statuswin, 1, 1))
      {
        case GET_COMMAND_OK:
          get_command_arguments(&commandarguments, commandbuffer);

          switch (identify_command(commandidentifier, commandbuffer, 0))
          {
            case COMMAND_SELECT_CONTAINING:
              cmd_select_containing(groups, totalgroups, commandarguments, status);
              break;

            case COMMAND_SELECT_BEGINNING:
              cmd_select_beginning(groups, totalgroups, commandarguments, status);
              break;

            case COMMAND_SELECT_ENDING:
              cmd_select_ending(groups, totalgroups, commandarguments, status);
              break;

            case COMMAND_SELECT_MATCHING:
              cmd_select_matching(groups, totalgroups, commandarguments, status);
              break;

            case COMMAND_SELECT_REGEX:
              cmd_select_regex(groups, totalgroups, commandarguments, status);
              break;

            case COMMAND_CLEAR_SELECTIONS_CONTAINING:
              cmd_clear_selections_containing(groups, totalgroups, commandarguments, status);
              break;

            case COMMAND_CLEAR_SELECTIONS_BEGINNING:
              cmd_clear_selections_beginning(groups, totalgroups, commandarguments, status);
              break;

            case COMMAND_CLEAR_SELECTIONS_ENDING:
              cmd_clear_selections_ending(groups, totalgroups, commandarguments, status);
              break;

            case COMMAND_CLEAR_SELECTIONS_MATCHING:
              cmd_clear_selections_matching(groups, totalgroups, commandarguments, status);
              break;

            case COMMAND_CLEAR_SELECTIONS_REGEX:
              cmd_clear_selections_regex(groups, totalgroups, commandarguments, status);
              break;

            case COMMAND_CLEAR_ALL_SELECTIONS:
              cmd_clear_all_selections(groups, totalgroups, commandarguments, status);
              break;

            case COMMAND_INVERT_GROUP_SELECTIONS:
              cmd_invert_group_selections(groups, totalgroups, commandarguments, status);
              break;

            case COMMAND_KEEP_SELECTED:
              cmd_keep_selected(groups, totalgroups, commandarguments, &globaldeletiontally, status);
              break;

            case COMMAND_DELETE_SELECTED:
              cmd_delete_selected(groups, totalgroups, commandarguments, &globaldeletiontally, status);
              break;

            case COMMAND_RESET_SELECTED:
              cmd_reset_selected(groups, totalgroups, commandarguments, &globaldeletiontally, status);
              break;

            case COMMAND_RESET_GROUP:
              for (x = 0; x < groups[cursorgroup].filecount; ++x)
                set_file_action(&groups[cursorgroup].files[x], 0, &globaldeletiontally);

              format_status_left(status, L"Reset all files in current group.");

              break;

            case COMMAND_PRESERVE_ALL:
              /* mark all files for preservation */
              for (x = 0; x < groups[cursorgroup].filecount; ++x)
                set_file_action(&groups[cursorgroup].files[x], 1, &globaldeletiontally);

              format_status_left(status, L"%d files marked for preservation", groups[cursorgroup].filecount);

              if (cursorgroup < totalgroups - 1)
                move_to_next_group(&topline, &cursorgroup, &cursorfile, groups, filewin);

              break;

            case COMMAND_GOTO_SET:
              number = wcstol(commandarguments, &wcstolcheck, 10);
              if (wcstolcheck != commandarguments && *wcstolcheck == '\0')
              {
                if (number >= 1 && number <= totalgroups)
                {
                  scroll_to_group(&topline, number - 1, 0, groups, filewin);

                  cursorgroup = number - 1;
                  cursorfile = 0;
                }
                else
                {
                  format_status_left(status, L"Group index out of range.");
                }
              }
              else
              {
                format_status_left(status, L"Invalid group index.");
              }

              break;

            case COMMAND_HELP:
              endwin();

              if (system(HELP_COMMAND_STRING) == -1)
                format_status_left(status, L"Could not display help text.");

              refresh();

              break;

            case COMMAND_EXIT: /* exit program */
              if (globaldeletiontally == 0)
              {
                doprune = 0;
                continue;
              }
              else
              {
                print_prompt(statuswin, L"[ There are files marked for deletion. Exit anyway? ]: ");

                switch (get_command_text(&commandbuffer, &commandbuffersize, statuswin, 0, 0))
                {
                  case GET_COMMAND_OK:
                    switch (identify_command(confirmationkeywordidentifier, commandbuffer, 0))
                    {
                      case COMMAND_YES:
                        doprune = 0;
                        continue;

                      case COMMAND_NO:
                      case COMMAND_UNDEFINED:
                        commandbuffer[0] = '\0';
                        continue;
                    }
                    break;

                  case GET_COMMAND_CANCELED:
                    commandbuffer[0] = '\0';
                    continue;

                  case GET_COMMAND_RESIZE_REQUESTED:
                    /* resize windows */
                    wresize(filewin, LINES - 2, COLS);

                    wresize(statuswin, 2, COLS);
                    mvwin(statuswin, LINES - 2, 0);

                    status_text_alloc(status, COLS);

                    /* recalculate line boundaries */
                    groupfirstline = 0;

                    for (g = 0; g < totalgroups; ++g)
                    {
                      groups[g].startline = groupfirstline;
                      groups[g].endline = groupfirstline + 2;

                      for (f = 0; f < groups[g].filecount; ++f)
                        groups[g].endline += filerowcount(groups[g].files[f].file, COLS, groups[g].filecount);

                      groupfirstline = groups[g].endline + 1;
                    }

                    commandbuffer[0] = '\0';

                    break;

                  case GET_COMMAND_ERROR_OUT_OF_MEMORY:
                    for (g = 0; g < totalgroups; ++g)
                      free(groups[g].files);

                    free(groups);
                    free(commandbuffer);
                    free_command_identifier_tree(commandidentifier);
                    free_command_identifier_tree(confirmationkeywordidentifier);

                    endwin();
                    errormsg("out of memory\n");
                    exit(1);
                    break;
                }
              }
              break;

            default: /* parse list of files to preserve and mark for preservation */
              intresult = validate_file_list(groups + cursorgroup, commandbuffer);
              if (intresult != FILE_LIST_OK)
              {
                if (intresult == FILE_LIST_ERROR_UNKNOWN_COMMAND)
                {
                  format_status_left(status, L"Unrecognized command");
                  break;
                }
                else if (intresult == FILE_LIST_ERROR_INDEX_OUT_OF_RANGE)
                {
                  format_status_left(status, L"Index out of range (1 - %d).", groups[cursorgroup].filecount);
                  break;
                }
                else if (intresult == FILE_LIST_ERROR_LIST_CONTAINS_INVALID_INDEX)
                {
                  format_status_left(status, L"Invalid index");
                  break;
                }
                else if (intresult == FILE_LIST_ERROR_OUT_OF_MEMORY)
                {
                  free(commandbuffer);

                  free_command_identifier_tree(commandidentifier);

                  for (g = 0; g < totalgroups; ++g)
                    free(groups[g].files);

                  free(groups);

                  endwin();
                  errormsg("out of memory\n");
                  exit(1);
                }
                else
                {
                  format_status_left(status, L"Could not interpret command");
                  break;
                }
              }

              token = wcstok(commandbuffer, L",", &wcsptr);

              while (token != NULL)
              {
                number = wcstol(token, &wcstolcheck, 10);
                if (wcstolcheck != token && *wcstolcheck == '\0')
                {
                  if (number > 0 && number <= groups[cursorgroup].filecount)
                    set_file_action(&groups[cursorgroup].files[number - 1], 1, &globaldeletiontally);
                }

                token = wcstok(NULL, L",", &wcsptr);
              }

              /* mark remaining files for deletion */
              preservecount = 0;
              deletecount = 0;

              for (x = 0; x < groups[cursorgroup].filecount; ++x)
              {
                if (groups[cursorgroup].files[x].action == 1)
                  ++preservecount;
                if (groups[cursorgroup].files[x].action == -1)
                  ++deletecount;
              }

              if (preservecount > 0)
              {
                for (x = 0; x < groups[cursorgroup].filecount; ++x)
                {
                  if (groups[cursorgroup].files[x].action == 0)
                  {
                    set_file_action(&groups[cursorgroup].files[x], -1, &globaldeletiontally);
                    ++deletecount;
                  }
                }
              }

              format_status_left(status, L"%d files marked for preservation, %d for deletion", preservecount, deletecount);

              if (cursorgroup < totalgroups - 1 && preservecount > 0)
                move_to_next_group(&topline, &cursorgroup, &cursorfile, groups, filewin);

              break;
          }

          break;

        case GET_COMMAND_KEY_SF:
          ++topline;

          resumecommandinput = 1;

          continue;

        case GET_COMMAND_KEY_SR:
          if (topline > 0)
            --topline;

          resumecommandinput = 1;

          continue;

        case GET_COMMAND_KEY_NPAGE:
          topline += getmaxy(filewin);

          resumecommandinput = 1;

          continue;

        case GET_COMMAND_KEY_PPAGE:
          topline -= getmaxy(filewin);

          if (topline < 0)
            topline = 0;

          resumecommandinput = 1;

          continue;

        case GET_COMMAND_CANCELED:
          continue;

        case GET_COMMAND_RESIZE_REQUESTED:
          /* resize windows */
          wresize(filewin, LINES - 2, COLS);

          wresize(statuswin, 2, COLS);
          mvwin(statuswin, LINES - 2, 0);

          status_text_alloc(status, COLS);

          /* recalculate line boundaries */
          groupfirstline = 0;

          for (g = 0; g < totalgroups; ++g)
          {
            groups[g].startline = groupfirstline;
            groups[g].endline = groupfirstline + 2;

            for (f = 0; f < groups[g].filecount; ++f)
              groups[g].endline += filerowcount(groups[g].files[f].file, COLS, groups[g].filecount);

            groupfirstline = groups[g].endline + 1;
          }

          commandbuffer[0] = '\0';

          break;

        case GET_COMMAND_ERROR_OUT_OF_MEMORY:
          for (g = 0; g < totalgroups; ++g)
            free(groups[g].files);

          free(groups);
          free(commandbuffer);
          free_command_identifier_tree(commandidentifier);
          free_command_identifier_tree(confirmationkeywordidentifier);

          endwin();
          errormsg("out of memory\n");
          exit(1);

          break;
      }

      commandbuffer[0] = '\0';
    }
    else if (keyresult == KEY_CODE_YES)
    {
      switch (wch)
      {
      case KEY_DOWN:
        if (cursorfile < groups[cursorgroup].filecount - 1)
          move_to_next_file(&topline, &cursorgroup, &cursorfile, groups, filewin);
        else if (cursorgroup < totalgroups - 1)
          move_to_next_group(&topline, &cursorgroup, &cursorfile, groups, filewin);

        break;

      case KEY_UP:
        if (cursorfile > 0)
          move_to_previous_file(&topline, &cursorgroup, &cursorfile, groups, filewin);
        else if (cursorgroup > 0)
          move_to_previous_group(&topline, &cursorgroup, &cursorfile, groups, filewin);

        break;

      case KEY_SF:
        ++topline;
        break;

      case KEY_SR:
        if (topline > 0)
          --topline;
        break;

      case KEY_NPAGE:
        topline += getmaxy(filewin);
        break;

      case KEY_PPAGE:
        topline -= getmaxy(filewin);

        if (topline < 0)
          topline = 0;

        break;

      case KEY_RIGHT:
        set_file_action(&groups[cursorgroup].files[cursorfile], 1, &globaldeletiontally);

        format_status_left(status, L"1 file marked for preservation.");

        if (cursorfile < groups[cursorgroup].filecount - 1)
          move_to_next_file(&topline, &cursorgroup, &cursorfile, groups, filewin);
        else if (cursorgroup < totalgroups - 1)
          move_to_next_group(&topline, &cursorgroup, &cursorfile, groups, filewin);

        break;

      case KEY_LEFT:
        deletecount = 0;

        set_file_action(&groups[cursorgroup].files[cursorfile], -1, &globaldeletiontally);

        format_status_left(status, L"1 file marked for deletion.");

        for (x = 0; x < groups[cursorgroup].filecount; ++x)
          if (groups[cursorgroup].files[x].action == -1)
            ++deletecount;

        if (deletecount < groups[cursorgroup].filecount)
        {
          if (cursorfile < groups[cursorgroup].filecount - 1)
            move_to_next_file(&topline, &cursorgroup, &cursorfile, groups, filewin);
          else if (cursorgroup < totalgroups - 1)
            move_to_next_group(&topline, &cursorgroup, &cursorfile, groups, filewin);
        }

        break;

      case KEY_BACKSPACE:
        if (cursorgroup > 0)
        {
          --cursorgroup;

          cursorfile = 0;

          scroll_to_group(&topline, cursorgroup, 0, groups, filewin);
        }
        break;

      case KEY_SRIGHT:
        move_to_next_selected_group(&topline, &cursorgroup, &cursorfile, groups, totalgroups, filewin);
        break;

      case KEY_SLEFT:
        move_to_previous_selected_group(&topline, &cursorgroup, &cursorfile, groups, totalgroups, filewin);
        break;

      case KEY_DC:
        totaldeleted = 0;
        deletedbytes = 0;

        for (g = 0; g < totalgroups; ++g)
        {
          preservecount = 0;
          deletecount = 0;
          unresolvedcount = 0;

          for (f = 0; f < groups[g].filecount; ++f)
          {
            switch (groups[g].files[f].action)
            {
              case -1:
                ++deletecount;
                break;
              case 0:
                ++unresolvedcount;
                break;
              case 1:
                ++preservecount;
                break;
            }
          }

          /* delete files marked for deletion unless no files left undeleted */
          if (deletecount < groups[g].filecount)
          {
            for (f = 0; f < groups[g].filecount; ++f)
            {
              if (groups[g].files[f].action == -1)
              {
                if (remove(groups[g].files[f].file->d_name) == 0)
                {
                  set_file_action(&groups[g].files[f], -2, &globaldeletiontally);

                  deletedbytes += groups[g].files[f].file->size;
                  ++totaldeleted;
                }
              }
            }

            deletecount = 0;
          }

          /* if no files left unresolved, mark preserved files for delisting */
          if (unresolvedcount == 0)
          {
            for (f = 0; f < groups[g].filecount; ++f)
              if (groups[g].files[f].action == 1)
                set_file_action(&groups[g].files[f], -2, &globaldeletiontally);

            preservecount = 0;
          }
          /* if only one file left unresolved, mark it for delesting */
          else if (unresolvedcount == 1 && preservecount + deletecount == 0)
          {
            for (f = 0; f < groups[g].filecount; ++f)
              if (groups[g].files[f].action == 0)
                set_file_action(&groups[g].files[f], -2, &globaldeletiontally);
          }

          /* delist any files marked for delisting */
          to = 0;
          for (f = 0; f < groups[g].filecount; ++f)
            if (groups[g].files[f].action != -2)
              groups[g].files[to++] = groups[g].files[f];

          groups[g].filecount = to;

          /* reposition cursor, if necessary */
          if (cursorgroup == g && cursorfile > 0 && cursorfile >= groups[g].filecount)
            cursorfile = groups[g].filecount - 1;
        }

        if (deletedbytes < 1000.0)
          format_status_left(status, L"Deleted %ld files (occupying %.0f bytes).", totaldeleted, deletedbytes);
        else if (deletedbytes <= (1000.0 * 1000.0))
          format_status_left(status, L"Deleted %ld files (occupying %.1f KB).", totaldeleted, deletedbytes / 1000.0);
        else if (deletedbytes <= (1000.0 * 1000.0 * 1000.0))
          format_status_left(status, L"Deleted %ld files (occupying %.1f MB).", totaldeleted, deletedbytes / (1000.0 * 1000.0));
        else
          format_status_left(status, L"Deleted %ld files (occupying %.1f GB).", totaldeleted, deletedbytes / (1000.0 * 1000.0 * 1000.0));

        /* delist empty groups */
        to = 0;
        for (g = 0; g < totalgroups; ++g)
        {
          if (groups[g].filecount > 0)
          {
            groups[to] = groups[g];

            /* reposition cursor, if necessary */
            if (to == cursorgroup && to != g)
              cursorfile = 0;

            ++to;
          }
        }

        totalgroups = to;

        /* reposition cursor, if necessary */
        if (cursorgroup >= totalgroups)
        {
          cursorgroup = totalgroups - 1;
          cursorfile = 0;
        }

        /* recalculate line boundaries */
        adjusttopline = 1;
        toplineoffset = 0;
        groupfirstline = 0;

        for (g = 0; g < totalgroups; ++g)
        {
          if (adjusttopline && groups[g].endline >= topline)
            toplineoffset = groups[g].endline - topline;

          groups[g].startline = groupfirstline;
          groups[g].endline = groupfirstline + 2;

          for (f = 0; f < groups[g].filecount; ++f)
            groups[g].endline += filerowcount(groups[g].files[f].file, COLS, groups[g].filecount);

          if (adjusttopline && toplineoffset > 0)
          {
            topline = groups[g].endline - toplineoffset;

            if (topline < 0)
              topline = 0;

            adjusttopline = 0;
          }

          groupfirstline = groups[g].endline + 1;
        }

        if (totalgroups > 0 && groups[totalgroups-1].endline <= topline)
        {
          topline = groups[totalgroups-1].endline - getmaxy(filewin) + 1;

          if (topline < 0)
            topline = 0;
        }

        break;

      case KEY_RESIZE:
        /* resize windows */
        wresize(filewin, LINES - 2, COLS);

        wresize(statuswin, 2, COLS);
        mvwin(statuswin, LINES - 2, 0);

        status_text_alloc(status, COLS);

        /* recalculate line boundaries */
        groupfirstline = 0;

        for (g = 0; g < totalgroups; ++g)
        {
          groups[g].startline = groupfirstline;
          groups[g].endline = groupfirstline + 2;

          for (f = 0; f < groups[g].filecount; ++f)
            groups[g].endline += filerowcount(groups[g].files[f].file, COLS, groups[g].filecount);

          groupfirstline = groups[g].endline + 1;
        }

        break;
      }
    }
    else if (keyresult == OK)
    {
      switch (wch)
      {
      case '?':
        if (groups[cursorgroup].files[cursorfile].action == 0)
          break;

        set_file_action(&groups[cursorgroup].files[cursorfile], 0, &globaldeletiontally);

        if (cursorfile < groups[cursorgroup].filecount - 1)
          move_to_next_file(&topline, &cursorgroup, &cursorfile, groups, filewin);
        else if (cursorgroup < totalgroups - 1)
          move_to_next_group(&topline, &cursorgroup, &cursorfile, groups, filewin);

        break;

      case '\n':
        deletecount = 0;
        preservecount = 0;

        for (x = 0; x < groups[cursorgroup].filecount; ++x)
        {
          if (groups[cursorgroup].files[x].action == 1)
            ++preservecount;
        }

        if (preservecount == 0)
          break;

        for (x = 0; x < groups[cursorgroup].filecount; ++x)
        {
          if (groups[cursorgroup].files[x].action == 0)
            set_file_action(&groups[cursorgroup].files[x], -1, &globaldeletiontally);

          if (groups[cursorgroup].files[x].action == -1)
            ++deletecount;
        }

        if (cursorgroup < totalgroups - 1 && deletecount < groups[cursorgroup].filecount)
          move_to_next_group(&topline, &cursorgroup, &cursorfile, groups, filewin);

        break;

      case '\t':
        if (cursorgroup < totalgroups - 1)
          move_to_next_group(&topline, &cursorgroup, &cursorfile, groups, filewin);

        break;
      }
    }
  } while (doprune);

  endwin();

  free(commandbuffer);

  free_command_identifier_tree(commandidentifier);

  for (g = 0; g < totalgroups; ++g)
    free(groups[g].files);

  free(groups);
}

void registerpair(file_t **matchlist, file_t *newmatch, 
		  int (*comparef)(file_t *f1, file_t *f2))
{
  file_t *traverse;
  file_t *back;

  (*matchlist)->hasdupes = 1;

  back = 0;
  traverse = *matchlist;
  while (traverse)
  {
    if (comparef(newmatch, traverse) <= 0)
    {
      newmatch->duplicates = traverse;
      
      if (back == 0)
      {
	*matchlist = newmatch; /* update pointer to head of list */

	newmatch->hasdupes = 1;
	traverse->hasdupes = 0; /* flag is only for first file in dupe chain */
      }
      else
	back->duplicates = newmatch;

      break;
    }
    else
    {
      if (traverse->duplicates == 0)
      {
	traverse->duplicates = newmatch;
	
	if (back == 0)
	  traverse->hasdupes = 1;
	
	break;
      }
    }
    
    back = traverse;
    traverse = traverse->duplicates;
  }
}

void help_text()
{
  printf("Usage: fdupes [options] DIRECTORY...\n\n");

  printf(" -r --recurse     \tfor every directory given follow subdirectories\n");
  printf("                  \tencountered within\n");
  printf(" -R --recurse:    \tfor each directory given after this option follow\n");
  printf("                  \tsubdirectories encountered within\n");
  printf(" -s --symlinks    \tfollow symlinks\n");
  printf(" -H --hardlinks   \tnormally, when two or more files point to the same\n");
  printf("                  \tdisk area they are treated as non-duplicates; this\n"); 
  printf("                  \toption will change this behavior\n");
  printf(" -n --noempty     \texclude zero-length files from consideration\n");
  printf(" -f --omitfirst   \tomit the first file in each set of matches\n");
  printf(" -1 --sameline    \tlist each set of matches on a single line\n");
  printf(" -S --size        \tshow size of duplicate files\n");
  printf(" -m --summarize   \tsummarize dupe information\n");
  printf(" -q --quiet       \thide progress indicator\n");
  printf(" -d --delete      \tprompt user for files to preserve and delete all\n"); 
  printf("                  \tothers; important: under particular circumstances,\n");
  printf("                  \tdata may be lost when using this option together\n");
  printf("                  \twith -s or --symlinks, or when specifying a\n");
  printf("                  \tparticular directory more than once; refer to the\n");
  printf("                  \tfdupes documentation for additional information\n");
  /*printf(" -l --relink      \t(description)\n");*/
  printf(" -N --noprompt    \ttogether with --delete, preserve the first file in\n");
  printf("                  \teach set of duplicates and delete the rest without\n");
  printf("                  \tprompting the user\n");
  printf(" -v --version     \tdisplay fdupes version\n");
  printf(" -h --help        \tdisplay this help message\n\n");
#ifdef OMIT_GETOPT_LONG
  printf("Note: Long options are not supported in this fdupes build.\n\n");
#endif
}

int main(int argc, char **argv) {
  int x;
  int opt;
  FILE *file1;
  FILE *file2;
  file_t *files = NULL;
  file_t *curfile;
  file_t **match = NULL;
  filetree_t *checktree = NULL;
  int filecount = 0;
  int progress = 0;
  char **oldargv;
  int firstrecurse;
  
#ifndef OMIT_GETOPT_LONG
  static struct option long_options[] = 
  {
    { "omitfirst", 0, 0, 'f' },
    { "recurse", 0, 0, 'r' },
    { "recursive", 0, 0, 'r' },
    { "recurse:", 0, 0, 'R' },
    { "recursive:", 0, 0, 'R' },
    { "quiet", 0, 0, 'q' },
    { "sameline", 0, 0, '1' },
    { "size", 0, 0, 'S' },
    { "symlinks", 0, 0, 's' },
    { "hardlinks", 0, 0, 'H' },
    { "relink", 0, 0, 'l' },
    { "noempty", 0, 0, 'n' },
    { "delete", 0, 0, 'd' },
    { "version", 0, 0, 'v' },
    { "help", 0, 0, 'h' },
    { "noprompt", 0, 0, 'N' },
    { "summarize", 0, 0, 'm'},
    { "summary", 0, 0, 'm' },
    { 0, 0, 0, 0 }
  };
#define GETOPT getopt_long
#else
#define GETOPT getopt
#endif

  program_name = argv[0];

  setlocale(LC_CTYPE, "");

  oldargv = cloneargs(argc, argv);

  while ((opt = GETOPT(argc, argv, "frRq1Ss::HlndvhNm"
#ifndef OMIT_GETOPT_LONG
          , long_options, NULL
#endif
          )) != EOF) {
    switch (opt) {
    case 'f':
      SETFLAG(flags, F_OMITFIRST);
      break;
    case 'r':
      SETFLAG(flags, F_RECURSE);
      break;
    case 'R':
      SETFLAG(flags, F_RECURSEAFTER);
      break;
    case 'q':
      SETFLAG(flags, F_HIDEPROGRESS);
      break;
    case '1':
      SETFLAG(flags, F_DSAMELINE);
      break;
    case 'S':
      SETFLAG(flags, F_SHOWSIZE);
      break;
    case 's':
      SETFLAG(flags, F_FOLLOWLINKS);
      break;
    case 'H':
      SETFLAG(flags, F_CONSIDERHARDLINKS);
      break;
    case 'n':
      SETFLAG(flags, F_EXCLUDEEMPTY);
      break;
    case 'd':
      SETFLAG(flags, F_DELETEFILES);
      break;
    case 'v':
      printf("fdupes %s\n", VERSION);
      exit(0);
    case 'h':
      help_text();
      exit(1);
    case 'N':
      SETFLAG(flags, F_NOPROMPT);
      break;
    case 'm':
      SETFLAG(flags, F_SUMMARIZEMATCHES);
      break;

    default:
      fprintf(stderr, "Try `fdupes --help' for more information.\n");
      exit(1);
    }
  }

  if (optind >= argc) {
    errormsg("no directories specified\n");
    exit(1);
  }

  if (ISFLAG(flags, F_RECURSE) && ISFLAG(flags, F_RECURSEAFTER)) {
    errormsg("options --recurse and --recurse: are not compatible\n");
    exit(1);
  }

  if (ISFLAG(flags, F_SUMMARIZEMATCHES) && ISFLAG(flags, F_DELETEFILES)) {
    errormsg("options --summarize and --delete are not compatible\n");
    exit(1);
  }

  if (ISFLAG(flags, F_RECURSEAFTER)) {
    firstrecurse = nonoptafter("--recurse:", argc, oldargv, argv, optind);
    
    if (firstrecurse == argc)
      firstrecurse = nonoptafter("-R", argc, oldargv, argv, optind);

    if (firstrecurse == argc) {
      errormsg("-R option must be isolated from other options\n");
      exit(1);
    }

    /* F_RECURSE is not set for directories before --recurse: */
    for (x = optind; x < firstrecurse; x++)
      filecount += grokdir(argv[x], &files);

    /* Set F_RECURSE for directories after --recurse: */
    SETFLAG(flags, F_RECURSE);

    for (x = firstrecurse; x < argc; x++)
      filecount += grokdir(argv[x], &files);
  } else {
    for (x = optind; x < argc; x++)
      filecount += grokdir(argv[x], &files);
  }

  if (!files) {
    if (!ISFLAG(flags, F_HIDEPROGRESS)) fprintf(stderr, "\r%40s\r", " ");
    exit(0);
  }
  
  curfile = files;

  while (curfile) {
    if (!checktree) 
      registerfile(&checktree, curfile);
    else 
      match = checkmatch(&checktree, checktree, curfile);

    if (match != NULL) {
      file1 = fopen(curfile->d_name, "rb");
      if (!file1) {
	curfile = curfile->next;
	continue;
      }
      
      file2 = fopen((*match)->d_name, "rb");
      if (!file2) {
	fclose(file1);
	curfile = curfile->next;
	continue;
      }

      if (confirmmatch(file1, file2)) {
	registerpair(match, curfile, sort_pairs_by_mtime);
	
	/*match->hasdupes = 1;
        curfile->duplicates = match->duplicates;
        match->duplicates = curfile;*/
      }
      
      fclose(file1);
      fclose(file2);
    }

    curfile = curfile->next;

    if (!ISFLAG(flags, F_HIDEPROGRESS)) {
      fprintf(stderr, "\rProgress [%d/%d] %d%% ", progress, filecount,
       (int)((float) progress / (float) filecount * 100.0));
      progress++;
    }
  }

  if (!ISFLAG(flags, F_HIDEPROGRESS)) fprintf(stderr, "\r%40s\r", " ");

  if (ISFLAG(flags, F_DELETEFILES))
  {
    if (ISFLAG(flags, F_NOPROMPT))
    {
      deletefiles(files, 0, 0);
    }
    else
    {
	deletefiles_ncurses(files);
	/*
      stdin = freopen("/dev/tty", "r", stdin);
      deletefiles(files, 1, stdin);
	*/
    }
  }

  else 

    if (ISFLAG(flags, F_SUMMARIZEMATCHES))
      summarizematches(files);
      
    else

      printmatches(files);

  while (files) {
    curfile = files->next;
    free(files->d_name);
    free(files->crcsignature);
    free(files->crcpartial);
    free(files);
    files = curfile;
  }

  for (x = 0; x < argc; x++)
    free(oldargv[x]);

  free(oldargv);

  purgetree(checktree);

  return 0;
}
