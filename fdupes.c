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
#include <ncurses.h>
#include "publicdomain/truefilename.h"
#include "publicdomain/fgetline.h"
#include "publicdomain/cursesprintsupport.h"

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
#define F_STDIN             0x1000

char *program_name;

unsigned long flags = 0;

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

file_t *describecandidate(char *file, file_t *next)
{
  file_t *newfile = (file_t*) malloc(sizeof(file_t));

  if (!newfile)
    return 0;

  newfile->d_name = file;
  newfile->next = next;
  newfile->device = 0;
  newfile->inode = 0;
  newfile->crcsignature = NULL;
  newfile->crcpartial = NULL;
  newfile->duplicates = NULL;
  newfile->hasdupes = 0;

  return newfile;
}

int grokpath(char *path, file_t **filelistp)
{
  DIR *cd;
  file_t *newfile;
  struct dirent *dirinfo;
  char *mallocedpath;
  string_t *truename;
  int lastchar;
  int filecount = 0;
  struct stat info;
  struct stat linfo;
  static int progress = 0;
  static char indicator[] = "-\\|/";

  if (stat(path, &info) == -1)
  {
    errormsg("could not stat %s\n", path);
    return 0;
  }

  if (lstat(path, &linfo) == -1)
  {
    errormsg("could not stat %s\n", path);
    return 0;
  }

  if (S_ISLNK(linfo.st_mode) && !S_ISDIR(info.st_mode))
  {
    truename = truefilename(path);

    filecount = grokpath(truename->buffer, filelistp);

    string_free(truename);

    return filecount;
  }

  if (S_ISREG(linfo.st_mode) && (filesize(path) != 0 || !ISFLAG(flags, F_EXCLUDEEMPTY)))
  {
    if (!ISFLAG(flags, F_HIDEPROGRESS)) {
      fprintf(stderr, "\rBuilding file list %c ", indicator[progress]);
      progress = (progress + 1) % 4;
    }

    /* path may be a command-line parameter, which cannot be
       free()'d, so use a malloc()'d copy instead. */
    mallocedpath = (char*)malloc(strlen(path)+1);
    if (!mallocedpath) {
      errormsg("out of memory!\n");
      exit(1);
    }
    strcpy(mallocedpath, path);
    
    newfile = describecandidate(mallocedpath, *filelistp);

    *filelistp = newfile;

    return 1;
  }  

  cd = opendir(path);

  if (!cd) {
    errormsg("could not chdir to %s\n", path);
    return 0;
  }

  while ((dirinfo = readdir(cd)) != NULL) {
    if (strcmp(dirinfo->d_name, ".") && strcmp(dirinfo->d_name, "..")) {
      if (!ISFLAG(flags, F_HIDEPROGRESS)) {
	fprintf(stderr, "\rBuilding file list %c ", indicator[progress]);
	progress = (progress + 1) % 4;
      }

      mallocedpath = (char*)malloc(strlen(path)+strlen(dirinfo->d_name)+2);
      if (!mallocedpath) {
	errormsg("out of memory!\n");
	closedir(cd);
	exit(1);
      }

      strcpy(mallocedpath, path);
      lastchar = strlen(mallocedpath) - 1;
      if (lastchar >= 0 && mallocedpath[lastchar] != '/')
	strcat(mallocedpath, "/");
      strcat(mallocedpath, dirinfo->d_name);

      newfile = describecandidate(mallocedpath, *filelistp);
      if (!newfile)
      {
	errormsg("out of memory!\n");
	free(mallocedpath);
	closedir(cd);
	exit(1);
      }
      
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
	  filecount += grokpath(newfile->d_name, filelistp);
	free(newfile->d_name);
	free(newfile);
      } else {
	if (S_ISREG(linfo.st_mode)) {
	  *filelistp = newfile;
	  filecount++;
	} else if (S_ISLNK(linfo.st_mode) && ISFLAG(flags, F_FOLLOWLINKS)) {
	  truename = truefilename(newfile->d_name);

	  filecount += grokpath(truename->buffer, filelistp);

	  string_free(truename);

	  free(newfile->d_name);
	  free(newfile);
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

/* If device and inode fields are equal one of the files is a 
   hard link to the other or the files have been listed twice 
   unintentionally. We don't want to flag these files as
   duplicates unless the user specifies otherwise.
*/
int excludehardlink(file_t *potentialmatch, file_t *newfile)
{
  file_t *traverse = potentialmatch;
  
  if (ISFLAG(flags, F_CONSIDERHARDLINKS))
    return 0;

  while (traverse)
  {
    if (newfile->device == traverse->device && 
	newfile->inode == traverse->inode)
      return 1;

    traverse = traverse->duplicates;
  }

  return 0;
}

int excludesamefile(file_t *potentialmatch, file_t *newfile)
{
  string_t *truename_newfile;
  string_t *truename_existing;
  file_t *traverse;

  truename_newfile = truefilename(newfile->d_name);

  traverse = potentialmatch;
  while (traverse)
  {
    truename_existing = truefilename(traverse->d_name);

    if (strcmp(truename_existing->buffer, truename_newfile->buffer) == 0)
    {
      printf("EXCLUDED: %s same as %s\n", newfile->d_name, traverse->d_name);
      string_free(truename_existing);
      string_free(truename_newfile);
      return 1;
    }

    string_free(truename_existing);

    traverse = traverse->duplicates;
  }

  string_free(truename_newfile);

  return 0;
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

struct deletegroupfile
{
  file_t *file;
  int preserve;
};

struct deletegroup
{
  struct deletegroupfile *files;
  size_t filecount;
  size_t startline;
};

size_t printgroup(int group, struct deletegroup *groups, int groupcount, int selectedgroup, int selectedfile, int quiet)
{
  int maxx;
  int maxy;
  size_t lines;
  size_t file;
  char preservechar;

  getmaxyx(curscr, maxy, maxx);

  lines = marginprintw(0, maxx, quiet, "Set %d of %d\n\n", group + 1, groupcount);

  for (file = 0; file < groups[group].filecount; ++file)
  {
    if (group == selectedgroup && file == selectedfile)
    {
      attron(A_BOLD);
      marginprintw(0, maxx-1, quiet, ">");
      attroff(A_BOLD);
    }

    preservechar = ' ';
    switch (groups[group].files[file].preserve)
    {
    case 0:
      preservechar = '-';
      break;

    case 1:
      preservechar = '+';
      break;
    }

    attron(A_BOLD);
    marginprintw(2, maxx-1, quiet, "%c", preservechar);
    attroff(A_BOLD);

    lines += marginprintw(4, maxx-1, quiet, "[%d] %s\n", file + 1, groups[group].files[file].file->d_name);
  }

  lines += marginprintw(0, maxx-1, quiet, "\n");

  return lines;
}

void deletefiles_ncurses(file_t *files)
{
  int selectedgroup = 0;
  int selectedfile = 0;
  file_t *tmpfile;
  file_t *curfile;
  struct deletegroup *groups = 0;
  size_t groupcount = 0;
  size_t group;
  size_t file;
  int topgroup;
  int topfile;
  int topline;
  int ch;
  int lines;
  int x;
  int y;
  int maxx;
  int maxy;
  int xnew;
  int ynew;

  curfile = files;
  
  while (curfile) {
    if (curfile->hasdupes) {
      ++groupcount;
      groups = realloc(groups, sizeof(struct deletegroup) * groupcount);

      groups[groupcount-1].files = 0;
      groups[groupcount-1].filecount = 0;

      tmpfile = curfile;
      while (tmpfile) {
	++groups[groupcount-1].filecount;
	
	groups[groupcount-1].files = realloc(groups[groupcount-1].files, groups[groupcount-1].filecount * sizeof(struct deletegroupfile));
	groups[groupcount-1].files[groups[groupcount-1].filecount-1].file = tmpfile;
	groups[groupcount-1].files[groups[groupcount-1].filecount-1].preserve = -1;

	tmpfile = tmpfile->duplicates;
      }
    }
    
    curfile = curfile->next;
  }

  initscr();
  noecho();
  cbreak();
  keypad(stdscr, 1);

  do {
    erase();
    for (group = 0; group < groupcount; ++group)
      lines = printgroup(group, groups, groupcount, selectedgroup, selectedfile, 0);

    refresh();
  
    ch = getch();

    switch (ch)
    {
    case '\n':
      if (selectedgroup+1 < groupcount)
      {
	++selectedgroup;
	selectedfile = 0;
      }
      break;

    case '\'':
      if (selectedgroup > 0)
      {
	--selectedgroup;
	selectedfile = 0;
      }
      break;

    case KEY_DOWN:
      if (selectedfile+1 < groups[selectedgroup].filecount)
	++selectedfile;
      else if (selectedgroup+1 < groupcount)
      {
	++selectedgroup;
	selectedfile = 0;
      }
      break;

    case KEY_UP:
      if (selectedfile > 0)
	--selectedfile;
      else if (selectedgroup > 0)
      {
	--selectedgroup;
	selectedfile = groups[selectedgroup].filecount - 1;
      }
      break;

    case KEY_LEFT:
      {
      if (groups[selectedgroup].files[selectedfile].preserve == -1)
      {
	size_t file;
	for (file = 0; file < groups[selectedgroup].filecount; ++file)
	  groups[selectedgroup].files[file].preserve = 1;
      }

      groups[selectedgroup].files[selectedfile].preserve = 0;
      }
      break;

    case KEY_RIGHT:
      if (groups[selectedgroup].files[selectedfile].preserve == -1)
      {
	for (file = 0; file < groups[selectedgroup].filecount; ++file)
	  groups[selectedgroup].files[file].preserve = 0;
      }
      
      groups[selectedgroup].files[selectedfile].preserve = 1;
      
      break;

    case 'a':
      for (file = 0; file < groups[selectedgroup].filecount; ++file)
	groups[selectedgroup].files[file].preserve = 1;
	
      if (selectedgroup+1 < groupcount)
      {
	selectedgroup++;
	selectedfile = 0;
      }
      break;

    case 'c':
      for (file = 0; file < groups[selectedgroup].filecount; ++file)
	groups[selectedgroup].files[file].preserve = -1;
      break;
    }

    move(0,0);
  } while (ch != 'q');

  endwin();
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
  printf("Usage: fdupes [options] FILE|DIRECTORY...\n\n");

  printf(" -i --stdin       \tget list of files/directories from standard input\n");
  printf("                  \tas well as the command line\n");
  printf(" -r --recurse     \tfor every directory given follow subdirectories\n");
  printf("                  \tencountered within\n");
  printf(" -R --recurse:    \tfor each directory given after this option follow\n");
  printf("                  \tsubdirectories encountered within\n");
  printf(" -s --symlinks    \tfollow symlinks; this setting has no effect on\n");
  printf("                  \tsymlinks given as command-line arguments or via\n");
  printf("                  \tstandard input, which are always followed\n");
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
    { "stdin", 0, 0, 'i' },
    { 0, 0, 0, 0 }
  };
#define GETOPT getopt_long
#else
#define GETOPT getopt
#endif

  program_name = argv[0];

  oldargv = cloneargs(argc, argv);

  while ((opt = GETOPT(argc, argv, "ifrRq1Ss::HlndvhNm"
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
    case 'i':
      SETFLAG(flags, F_STDIN);
      break;

    default:
      fprintf(stderr, "Try `fdupes --help' for more information.\n");
      exit(1);
    }
  }

  if (optind >= argc && !ISFLAG(flags, F_STDIN)) {
    errormsg("no files specified\n");
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
      filecount += grokpath(argv[x], &files);

    /* Set F_RECURSE for directories after --recurse: */
    SETFLAG(flags, F_RECURSE);

    for (x = firstrecurse; x < argc; x++)
      filecount += grokpath(argv[x], &files);
  } else {
    for (x = optind; x < argc; x++)
      filecount += grokpath(argv[x], &files);
  }
  
  if (ISFLAG(flags, F_STDIN))
  {
    string_t *linebuffer = string_init();
 
    char *line = fgetline(stdin, linebuffer);
    while (line)
    {
      if (line[0] != '\0')
	filecount += grokpath(line, &files);
      
      line = fgetline(stdin, linebuffer);
    }
    
    string_free(linebuffer);
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
      if (excludehardlink(*match, curfile))
      {
	curfile = curfile->next;
	continue;
      }

      if (excludesamefile(*match, curfile))
      {
	curfile = curfile->next;
	continue;
      }
      
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
    {/*
      FILE *tty = fopen("/dev/tty", "r");
      if (!tty)
      {
	errormsg("could not read from terminal!");
	exit(1);
      }*/

      /*deletefiles(files, 1, tty);*/
      stdin = freopen("/dev/tty", "r", stdin);
      deletefiles_ncurses(files);

      /*fclose(tty);*/
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
