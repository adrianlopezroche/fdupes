/* FDUPES Copyright (c) 1999-2018 Adrian Lopez

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

#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <errno.h>
#include <libgen.h>
#include <locale.h>
#ifndef NO_NCURSES
#ifdef HAVE_NCURSESW_CURSES_H
  #include <ncursesw/curses.h>
#else
  #include <curses.h>
#endif
#include "ncurses-interface.h"
#endif
#include "fdupes.h"
#include "confirmmatch.h"
#include "errormsg.h"
#include "log.h"
#include "sigint.h"
#include "flags.h"
#include "removeifnotchanged.h"

long long minsize = -1;
long long maxsize = -1;

typedef enum {
  ORDER_MTIME = 0,
  ORDER_CTIME,
  ORDER_NAME
} ordertype_t;

char *program_name;

ordertype_t ordertype = ORDER_MTIME;

#define MD5_DIGEST_LENGTH 16

typedef struct _filetree {
  file_t *file; 
  struct _filetree *left;
  struct _filetree *right;
} filetree_t;

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

char *fmttime(time_t t) {
  static char buf[64];

  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", localtime(&t));

  return buf;
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

void getfilestats(file_t *file, struct stat *info, struct stat *linfo)
{
  file->size = info->st_size;;
  file->inode = info->st_ino;
  file->device = info->st_dev;
  file->ctime = info->st_ctime;
  file->mtime = info->st_mtime;
}

int grokdir(char *dir, file_t **filelistp, struct stat *logfile_status)
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
  char *fullname, *name;

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
      
      if (ISFLAG(flags, F_EXCLUDEHIDDEN)) {
	fullname = strdup(newfile->d_name);
	if (fullname == 0)
	{
	  errormsg("out of memory!\n");
	  free(newfile);
	  closedir(cd);
	  exit(1);
	}
	name = basename(fullname);
	if (name[0] == '.' && strcmp(name, ".") && strcmp(name, "..") ) {
	  free(newfile->d_name);
	  free(newfile);
	  free(fullname);
	  continue;
	}
	free(fullname);
      }

      if (stat(newfile->d_name, &info) == -1) {
        free(newfile->d_name);
        free(newfile);
        continue;
      }
      
      if (!S_ISDIR(info.st_mode) && (((info.st_size == 0 && ISFLAG(flags, F_EXCLUDEEMPTY)) || info.st_size < minsize || (info.st_size > maxsize && maxsize != -1)))) {
        free(newfile->d_name);
        free(newfile);
        continue;
      }

      /* ignore logfile */
      if (info.st_dev == logfile_status->st_dev && info.st_ino == logfile_status->st_ino)
      {
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
	  filecount += grokdir(newfile->d_name, filelistp, logfile_status);
	free(newfile->d_name);
	free(newfile);
      } else {
	if (S_ISREG(linfo.st_mode) || (S_ISLNK(linfo.st_mode) && ISFLAG(flags, F_FOLLOWLINKS))) {
	  getfilestats(newfile, &info, &linfo);
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

md5_byte_t *getcrcsignatureuntil(char *filename, off_t fsize, off_t max_read)
{
  off_t toread;
  md5_state_t state;
  static md5_byte_t digest[MD5_DIGEST_LENGTH];  
  static md5_byte_t chunk[CHUNK_SIZE];
  FILE *file;
   
  md5_init(&state);
  
  if (max_read != 0 && fsize > max_read)
    fsize = max_read;

  file = fopen(filename, "rb");
  if (file == NULL) {
    errormsg("error opening file %s\n", filename);
    return NULL;
  }
 
  while (fsize > 0) {
    toread = (fsize >= CHUNK_SIZE) ? CHUNK_SIZE : fsize;
    if (fread(chunk, toread, 1, file) != 1) {
      errormsg("error reading from file %s\n", filename);
      fclose(file);
      return NULL;
    }
    md5_append(&state, chunk, toread);
    fsize -= toread;
  }

  md5_finish(&state, digest);

  fclose(file);

  return digest;
}

md5_byte_t *getcrcsignature(char *filename, off_t fsize)
{
  return getcrcsignatureuntil(filename, fsize, 0);
}

md5_byte_t *getcrcpartialsignature(char *filename, off_t fsize)
{
  return getcrcsignatureuntil(filename, fsize, PARTIAL_MD5_SIZE);
}

int md5cmp(const md5_byte_t *a, const md5_byte_t *b)
{
  int x;

  for (x = 0; x < MD5_DIGEST_LENGTH; ++x)
  {
    if (a[x] < b[x])
      return -1;
    else if (a[x] > b[x])
      return 1;
  }

  return 0;
}

void md5copy(md5_byte_t *to, const md5_byte_t *from)
{
  int x;

  for (x = 0; x < MD5_DIGEST_LENGTH; ++x)
    to[x] = from[x];
}

void purgetree(filetree_t *checktree)
{
  if (checktree->left != NULL) purgetree(checktree->left);
    
  if (checktree->right != NULL) purgetree(checktree->right);
    
  free(checktree);
}

int registerfile(filetree_t **branch, file_t *file)
{
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

int same_permissions(char* name1, char* name2)
{
    struct stat s1, s2;

    if (stat(name1, &s1) != 0) return -1;
    if (stat(name2, &s2) != 0) return -1;

    return (s1.st_mode == s2.st_mode &&
            s1.st_uid == s2.st_uid &&
            s1.st_gid == s2.st_gid);
}

int is_hardlink(filetree_t *checktree, file_t *file)
{
  file_t *dupe;

  if ((file->inode == checktree->file->inode) &&
      (file->device == checktree->file->device))
        return 1;

  if (checktree->file->hasdupes)
  {
    dupe = checktree->file->duplicates;

    do {
      if ((file->inode == dupe->inode) &&
          (file->device == dupe->device))
            return 1;

      dupe = dupe->duplicates;
    } while (dupe != NULL);
  }

  return 0;
}

/* check whether two paths represent the same file (deleting one would delete the other) */
int is_same_file(file_t *file_a, file_t *file_b)
{
  char *filename_a;
  char *filename_b;
  char *dirname_a;
  char *dirname_b;
  char *basename_a;
  char *basename_b;
  struct stat dirstat_a;
  struct stat dirstat_b;

  /* if files on different devices and/or different inodes, they are not the same file */
  if (file_a->device != file_b->device || file_a->inode != file_b->inode)
    return 0;

  /* copy filenames (basename and dirname may modify these) */
  filename_a = strdup(file_a->d_name);
  if (filename_a == 0)
    return -1;

  filename_b = strdup(file_b->d_name);
  if (filename_b == 0)
    return -1;

  /* get file basenames */
  basename_a = basename(filename_a);
  memmove(filename_a, basename_a, strlen(basename_a) + 1);

  basename_b = basename(filename_b);
  memmove(filename_b, basename_b, strlen(basename_b) + 1);

  /* if files have different names, they are not the same file */
  if (strcmp(filename_a, filename_b) != 0)
  {
    free(filename_b);
    free(filename_a);
    return 0;
  }

  /* restore paths */
  strcpy(filename_a, file_a->d_name);
  strcpy(filename_b, file_b->d_name);

  /* get directory names */
  dirname_a = dirname(filename_a);
  if (stat(dirname_a, &dirstat_a) != 0)
  {
    free(filename_b);
    free(filename_a);
    return -1;
  }

  dirname_b = dirname(filename_b);
  if (stat(dirname_b, &dirstat_b) != 0)
  {
    free(filename_b);
    free(filename_a);
    return -1;
  }

  free(filename_b);
  free(filename_a);

  /* if directories on which files reside are different, they are not the same file */
  if (dirstat_a.st_dev != dirstat_b.st_dev || dirstat_a.st_ino != dirstat_b.st_ino)
    return 0;

  /* same device, inode, filename, and directory; therefore, same file */
  return 1;
}

/* check whether given tree node already contains a copy of given file */
int has_same_file(filetree_t *checktree, file_t *file)
{
  file_t *dupe;

  if (is_same_file(checktree->file, file))
    return 1;

  if (checktree->file->hasdupes)
  {
    dupe = checktree->file->duplicates;

    do {
      if (is_same_file(dupe, file))
        return 1;

      dupe = dupe->duplicates;
    } while (dupe != NULL);
  }

  return 0;
}

file_t **checkmatch(filetree_t **root, filetree_t *checktree, file_t *file)
{
  int cmpresult;
  md5_byte_t *crcsignature;

  if (ISFLAG(flags, F_CONSIDERHARDLINKS))
  {
    /* If node already contains file, we don't want to add it again.
    */
    if (has_same_file(checktree, file))
      return NULL;
  }
  else
  {
    /* If device and inode fields are equal one of the files is a
       hard link to the other or the files have been listed twice
       unintentionally. We don't want to flag these files as
       duplicates unless the user specifies otherwise.
    */
    if (is_hardlink(checktree, file))
      return NULL;
  }
  
  if (file->size < checktree->file->size)
    cmpresult = -1;
  else 
    if (file->size > checktree->file->size) cmpresult = 1;
  else
    if (ISFLAG(flags, F_PERMISSIONS) &&
        !same_permissions(file->d_name, checktree->file->d_name))
        cmpresult = -1;
  else {
    if (checktree->file->crcpartial == NULL) {
      crcsignature = getcrcpartialsignature(checktree->file->d_name, checktree->file->size);
      if (crcsignature == NULL) {
        errormsg ("cannot read file %s\n", checktree->file->d_name);
        return NULL;
      }

      checktree->file->crcpartial = (md5_byte_t*) malloc(MD5_DIGEST_LENGTH * sizeof(md5_byte_t));
      if (checktree->file->crcpartial == NULL) {
	errormsg("out of memory\n");
	exit(1);
      }
      md5copy(checktree->file->crcpartial, crcsignature);
    }

    if (file->crcpartial == NULL) {
      crcsignature = getcrcpartialsignature(file->d_name, file->size);
      if (crcsignature == NULL) {
        errormsg ("cannot read file %s\n", file->d_name);
        return NULL;
      }

      file->crcpartial = (md5_byte_t*) malloc(MD5_DIGEST_LENGTH * sizeof(md5_byte_t));
      if (file->crcpartial == NULL) {
	errormsg("out of memory\n");
	exit(1);
      }
      md5copy(file->crcpartial, crcsignature);
    }

    cmpresult = md5cmp(file->crcpartial, checktree->file->crcpartial);

    if (cmpresult == 0) {
      if (checktree->file->crcsignature == NULL) {
	crcsignature = getcrcsignature(checktree->file->d_name, checktree->file->size);
	if (crcsignature == NULL) return NULL;

	checktree->file->crcsignature = (md5_byte_t*) malloc(MD5_DIGEST_LENGTH * sizeof(md5_byte_t));
	if (checktree->file->crcsignature == NULL) {
	  errormsg("out of memory\n");
	  exit(1);
	}
	md5copy(checktree->file->crcsignature, crcsignature);
      }

      if (file->crcsignature == NULL) {
	crcsignature = getcrcsignature(file->d_name, file->size);
	if (crcsignature == NULL) return NULL;

	file->crcsignature = (md5_byte_t*) malloc(MD5_DIGEST_LENGTH * sizeof(md5_byte_t));
	if (file->crcsignature == NULL) {
	  errormsg("out of memory\n");
	  exit(1);
	}
	md5copy(file->crcsignature, crcsignature);
      }

      cmpresult = md5cmp(file->crcsignature, checktree->file->crcsignature);
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
    return &checktree->file;
  }
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
      printf("%d duplicate files (in %d sets), occupying %.1f kilobytes\n\n", numfiles, numsets, numbytes / 1000.0);
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
	if (ISFLAG(flags, F_SHOWSIZE)) printf("%lld byte%seach:\n", (long long int)files->size,
	 (files->size != 1) ? "s " : " ");
        if (ISFLAG(flags, F_SHOWTIME))
          printf("%s ", fmttime(files->mtime));
	if (ISFLAG(flags, F_DSAMELINE)) escapefilename("\\ ", &files->d_name);
	printf("%s%c", files->d_name, ISFLAG(flags, F_DSAMELINE)?' ':'\n');
      }
      tmpfile = files->duplicates;
      while (tmpfile != NULL) {
        if (ISFLAG(flags, F_SHOWTIME))
          printf("%s ", fmttime(tmpfile->mtime));
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

void deletefiles(file_t *files, int prompt, FILE *tty, char *logfile)
{
  int counter;
  int groups = 0;
  int curgroup = 0;
  file_t *tmpfile;
  file_t *curfile;
  file_t **dupelist;
  int *preserve;
  int firstpreserved;
  char *preservestr;
  char *token;
  char *tstr;
  int number;
  int sum;
  int max = 0;
  int x;
  int i;
  struct log_info *loginfo;
  int log_error;
  FILE *file1;
  FILE *file2;
  int ismatch;
  char *errorstring;

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

  loginfo = 0;
  if (logfile != 0)
    loginfo = log_open(logfile, &log_error);

  register_sigint_handler();

  while (files) {
    if (files->hasdupes) {
      curgroup++;
      counter = 1;
      dupelist[counter] = files;

      if (prompt) 
      {
        if (ISFLAG(flags, F_SHOWTIME))
          printf("[%d] [%s] %s\n", counter, fmttime(files->mtime), files->d_name);
        else
          printf("[%d] %s\n", counter, files->d_name);
      }

      tmpfile = files->duplicates;

      while (tmpfile) {
	dupelist[++counter] = tmpfile;
        if (prompt)
        {
          if (ISFLAG(flags, F_SHOWTIME))
            printf("[%d] [%s] %s\n", counter, fmttime(tmpfile->mtime), tmpfile->d_name);
          else
            printf("[%d] %s\n", counter, tmpfile->d_name);
        }
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
	printf("Set %d of %d, preserve files [1 - %d, all, quit]",
          curgroup, groups, counter);
	if (ISFLAG(flags, F_SHOWSIZE)) printf(" (%lld byte%seach)", (long long int)files->size,
	  (files->size != 1) ? "s " : " ");
	printf(": ");
	fflush(stdout);

	if (!fgets(preservestr, INPUT_SIZE, tty))
	{
	  preservestr[0] = '\n'; /* treat fgets() failure as if nothing was entered */
	  preservestr[1] = '\0';

	  if (got_sigint)
	  {
	    if (loginfo)
	      log_close(loginfo);

	    free(dupelist);
	    free(preserve);
	    free(preservestr);

	    printf("\n");

	    exit(0);
	  }
	}

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
	    preservestr[1] = '\0';
	    break;
	  }
	  i = strlen(preservestr)-1;
	}

	if (strcmp(preservestr, "q\n") == 0 || strcmp(preservestr, "quit\n") == 0)
	{
	  if (loginfo)
	    log_close(loginfo);

	  free(dupelist);
	  free(preserve);
	  free(preservestr);

	  printf("\n");

	  exit(0);
	}

	for (x = 1; x <= counter; x++) preserve[x] = 0;
	
	token = strtok(preservestr, " ,\n");
	
	while (token != NULL) {
	  if (strcasecmp(token, "all") == 0 || strcasecmp(token, "a") == 0)
	    for (x = 0; x <= counter; x++) preserve[x] = 1;
	  
	  number = 0;
	  sscanf(token, "%d", &number);
	  if (number > 0 && number <= counter) preserve[number] = 1;
	  
	  token = strtok(NULL, " ,\n");
	}
      
	for (sum = 0, x = 1; x <= counter; x++) sum += preserve[x];
      } while (sum < 1); /* make sure we've preserved at least one file */

      printf("\n");

      if (loginfo)
        log_begin_set(loginfo);

      for (x = 1; x <= counter; x++) { 
	if (preserve[x])
        {
	  printf("   [+] %s\n", dupelist[x]->d_name);

          if (loginfo)
            log_file_remaining(loginfo, dupelist[x]->d_name);
        }
	else {
    if (ISFLAG(flags, F_DEFERCONFIRMATION))
    {
      firstpreserved = 0;
      for (i = 1; i <= counter; ++i)
      {
        if (preserve[i])
        {
          firstpreserved = i;
          break;
        }
      }

      file1 = fopen(dupelist[x]->d_name, "rb");
      file2 = fopen(dupelist[firstpreserved]->d_name, "rb");

      if (file1 && file2)
        ismatch = confirmmatch(file1, file2);
      else
        ismatch = 0;

      if (file2)
        fclose(file2);

      if (file1)
        fclose(file1);
    }
    else
    {
      ismatch = 1;
    }

    if (ismatch) {
      if (removeifnotchanged(dupelist[x], &errorstring) == 0) {
        printf("   [-] %s\n", dupelist[x]->d_name);

        if (loginfo)
          log_file_deleted(loginfo, dupelist[x]->d_name);
      }
      else {
        printf("   [!] %s ", dupelist[x]->d_name);
        printf("-- unable to delete file: %s!\n", errorstring);

        if (loginfo)
          log_file_remaining(loginfo, dupelist[x]->d_name);
      }
    }
    else {
      printf("   [!] %s\n", dupelist[x]->d_name);
      printf(" -- unable to confirm match; file not deleted!\n");

      if (loginfo)
        log_file_remaining(loginfo, dupelist[x]->d_name);
    }
	}
      }
      printf("\n");

      if (loginfo)
        log_end_set(loginfo);
    }
    
    files = files->next;
  }

  if (loginfo)
    log_close(loginfo);

  free(dupelist);
  free(preserve);
  free(preservestr);
}

int sort_pairs_by_arrival(file_t *f1, file_t *f2)
{
  if (f2->duplicates != 0)
    return !ISFLAG(flags, F_REVERSE) ? 1 : -1;

  return !ISFLAG(flags, F_REVERSE) ? -1 : 1;
}

int sort_pairs_by_ctime(file_t *f1, file_t *f2)
{
  if (f1->ctime < f2->ctime)
    return !ISFLAG(flags, F_REVERSE) ? -1 : 1;
  else if (f1->ctime > f2->ctime)
    return !ISFLAG(flags, F_REVERSE) ? 1 : -1;

  return 0;
}

int sort_pairs_by_mtime(file_t *f1, file_t *f2)
{
  if (f1->mtime < f2->mtime)
    return !ISFLAG(flags, F_REVERSE) ? -1 : 1;
  else if (f1->mtime > f2->mtime)
    return !ISFLAG(flags, F_REVERSE) ? 1 : -1;
  else
    return sort_pairs_by_ctime(f1, f2);
}

int sort_pairs_by_filename(file_t *f1, file_t *f2)
{
  int strvalue = strcmp(f1->d_name, f2->d_name);
  return !ISFLAG(flags, F_REVERSE) ? strvalue : -strvalue;
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

void deletesuccessor(file_t **existing, file_t *duplicate, int matchconfirmed,
      int (*comparef)(file_t *f1, file_t *f2), struct log_info *loginfo)
{
  file_t *to_keep;
  file_t *to_delete;
  char *errorstring;

  if (comparef(duplicate, *existing) >= 0)
  {
    to_keep = *existing;
    to_delete = duplicate;
  }
  else
  {
    to_keep = duplicate;
    to_delete = *existing;

    *existing = duplicate;
  }

  if (!ISFLAG(flags, F_HIDEPROGRESS)) fprintf(stderr, "\r%40s\r", " ");

  if (loginfo)
    log_begin_set(loginfo);

  printf("   [+] %s\n", to_keep->d_name);

  if (loginfo)
    log_file_remaining(loginfo, to_keep->d_name);

  if (matchconfirmed)
  {
    if (removeifnotchanged(to_delete, &errorstring) == 0) {
      printf("   [-] %s\n", to_delete->d_name);

      if (loginfo)
        log_file_deleted(loginfo, to_delete->d_name);
    } else {
      printf("   [!] %s ", to_delete->d_name);
      printf("-- unable to delete file: %s!\n", errorstring);

      if (loginfo)
        log_file_remaining(loginfo, to_delete->d_name);
    }
  }
  else
  {
    printf("   [!] %s\n", to_delete->d_name);
    printf(" -- unable to confirm match; file not deleted!\n");

    if (loginfo)
      log_file_remaining(loginfo, to_delete->d_name);
  }

  if (loginfo)
    log_end_set(loginfo);

  printf("\n");
}

void help_text()
{
  printf("Usage: fdupes [options] DIRECTORY...\n\n");

  /*     0        1 0       2 0       3 0       4 0       5 0       6 0       7 0       8 0
  -------"---------|---------|---------|---------|---------|---------|---------|---------|"
  */
  printf(" -r --recurse            for every directory given follow subdirectories\n");
  printf("                         encountered within\n");
  printf(" -R --recurse:           for each directory given after this option follow\n");
  printf("                         subdirectories encountered within (note the ':' at the\n");
  printf("                         end of the option, manpage for more details)\n");
  printf(" -s --symlinks           follow symlinks\n");
  printf(" -H --hardlinks          normally, when two or more files point to the same\n");
  printf("                         disk area they are treated as non-duplicates; this\n");
  printf("                         option will change this behavior\n");
  printf(" -G --minsize=SIZE       consider only files greater than or equal to SIZE bytes\n");
  printf(" -L --maxsize=SIZE       consider only files less than or equal to SIZE bytes\n");
  printf(" -n --noempty            exclude zero-length files from consideration\n");
  printf(" -A --nohidden           exclude hidden files from consideration\n");
  printf(" -f --omitfirst          omit the first file in each set of matches\n");
  printf(" -1 --sameline           list each set of matches on a single line\n");
  printf(" -S --size               show size of duplicate files\n");
  printf(" -t --time               show modification time of duplicate files\n");
  printf(" -m --summarize          summarize dupe information\n");
  printf(" -q --quiet              hide progress indicator\n");
  printf(" -d --delete             prompt user for files to preserve and delete all\n");
  printf("                         others; important: under particular circumstances,\n");
  printf("                         data may be lost when using this option together\n");
  printf("                         with -s or --symlinks, or when specifying a\n");
  printf("                         particular directory more than once; refer to the\n");
  printf("                         fdupes documentation for additional information\n");
  printf(" -D --deferconfirmation  in interactive mode, defer byte-for-byte confirmation\n");
  printf("                         of duplicates until just before file deletion\n");
#ifndef NO_NCURSES
  printf(" -P --plain              with --delete, use line-based prompt (as with older\n");
  printf("                         versions of fdupes) instead of screen-mode interface\n");
#endif
  printf(" -N --noprompt           together with --delete, preserve the first file in\n");
  printf("                         each set of duplicates and delete the rest without\n");
  printf("                         prompting the user\n");
  printf(" -I --immediate          delete duplicates as they are encountered, without\n");
  printf("                         grouping into sets; implies --noprompt\n");
  printf(" -p --permissions        don't consider files with different owner/group or\n");
  printf("                         permission bits as duplicates\n");
  printf(" -o --order=BY           select sort order for output and deleting; by file\n");
  printf("                         modification time (BY='time'; default), status\n");
  printf("                         change time (BY='ctime'), or filename (BY='name')\n");
  printf(" -i --reverse            reverse order while sorting\n");
  printf(" -l --log=LOGFILE        log file deletion choices to LOGFILE\n");
  printf(" -v --version            display fdupes version\n");
  printf(" -h --help               display this help message\n\n");
#ifndef HAVE_GETOPT_H
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
  char *logfile = 0;
  struct log_info *loginfo = NULL;
  int log_error;
  struct stat logfile_status;
  char *endptr;
  
#ifdef HAVE_GETOPT_H
  static struct option long_options[] = 
  {
    { "omitfirst", 0, 0, 'f' },
    { "recurse", 0, 0, 'r' },
    { "recurse:", 0, 0, 'R' },
    { "quiet", 0, 0, 'q' },
    { "sameline", 0, 0, '1' },
    { "size", 0, 0, 'S' },
    { "time", 0, 0, 't' },
    { "symlinks", 0, 0, 's' },
    { "hardlinks", 0, 0, 'H' },
    { "minsize", 1, 0, 'G' },
    { "maxsize", 1, 0, 'L' },
    { "noempty", 0, 0, 'n' },
    { "nohidden", 0, 0, 'A' },
    { "delete", 0, 0, 'd' },
    { "plain", 0, 0, 'P' },
    { "version", 0, 0, 'v' },
    { "help", 0, 0, 'h' },
    { "noprompt", 0, 0, 'N' },
    { "immediate", 0, 0, 'I'},
    { "summarize", 0, 0, 'm'},
    { "summary", 0, 0, 'm' },
    { "permissions", 0, 0, 'p' },
    { "order", 1, 0, 'o' },
    { "reverse", 0, 0, 'i' },
    { "log", 1, 0, 'l' },
    { "deferconfirmation", 0, 0, 'D' },
    { 0, 0, 0, 0 }
  };
#define GETOPT getopt_long
#else
#define GETOPT getopt
#endif

  program_name = argv[0];

  setlocale(LC_CTYPE, "");

  oldargv = cloneargs(argc, argv);

  while ((opt = GETOPT(argc, argv, "frRq1StsHG:L:nAdPvhNImpo:il:D"
#ifdef HAVE_GETOPT_H
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
    case 't':
      SETFLAG(flags, F_SHOWTIME);
      break;
    case 's':
      SETFLAG(flags, F_FOLLOWLINKS);
      break;
    case 'H':
      SETFLAG(flags, F_CONSIDERHARDLINKS);
      break;
    case 'G':
      minsize = strtoll(optarg, &endptr, 10);
      if (optarg[0] == '\0' || *endptr != '\0' || minsize < 0)
      {
        errormsg("invalid value for --minsize: '%s'\n", optarg);
        exit(1);
      }
      break;
    case 'L':
      maxsize = strtoll(optarg, &endptr, 10);
      if (optarg[0] == '\0' || *endptr != '\0' || maxsize < 0)
      {
        errormsg("invalid value for --maxsize: '%s'\n", optarg);
        exit(1);
      }
      break;
    case 'n':
      SETFLAG(flags, F_EXCLUDEEMPTY);
      break;
    case 'A':
      SETFLAG(flags, F_EXCLUDEHIDDEN);
      break;
    case 'd':
      SETFLAG(flags, F_DELETEFILES);
      break;
    case 'P':
      SETFLAG(flags, F_PLAINPROMPT);
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
    case 'I':
      SETFLAG(flags, F_IMMEDIATE);
      break;
    case 'm':
      SETFLAG(flags, F_SUMMARIZEMATCHES);
      break;
    case 'p':
      SETFLAG(flags, F_PERMISSIONS);
      break;
    case 'o':
      if (!strcasecmp("name", optarg)) {
        ordertype = ORDER_NAME;
      } else if (!strcasecmp("time", optarg)) {
        ordertype = ORDER_MTIME;
      } else if (!strcasecmp("ctime", optarg)) {
        ordertype = ORDER_CTIME;
      } else {
        errormsg("invalid value for --order: '%s'\n", optarg);
        exit(1);
      }
      break;
    case 'i':
      SETFLAG(flags, F_REVERSE);
      break;
    case 'l':
      logfile = optarg;
      break;
    case 'D':
      SETFLAG(flags, F_DEFERCONFIRMATION);
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

  if (ISFLAG(flags, F_DEFERCONFIRMATION) && (!ISFLAG(flags, F_DELETEFILES) || ISFLAG(flags, F_NOPROMPT)))
  {
    errormsg("--deferconfirmation only works with interactive deletion modes\n");
    exit(1);
  }

  if (!ISFLAG(flags, F_DELETEFILES))
    logfile = 0;

  if (logfile != 0)
  {
    loginfo = log_open(logfile, &log_error);
    if (loginfo == 0)
    {
      if (log_error == LOG_ERROR_NOT_A_LOG_FILE)
        errormsg("%s: doesn't look like an fdupes log file\n", logfile);
      else
        errormsg("%s: could not open log file\n", logfile);

      exit(1);
    }

    if (stat(logfile, &logfile_status) != 0)
    {
      errormsg("could not read log file status\n");
      exit(1);
    }
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
      filecount += grokdir(argv[x], &files, &logfile_status);

    /* Set F_RECURSE for directories after --recurse: */
    SETFLAG(flags, F_RECURSE);

    for (x = firstrecurse; x < argc; x++)
      filecount += grokdir(argv[x], &files, &logfile_status);
  } else {
    for (x = optind; x < argc; x++)
      filecount += grokdir(argv[x], &files, &logfile_status);
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

      if (ISFLAG(flags, F_DELETEFILES) && ISFLAG(flags, F_IMMEDIATE))
      {
          deletesuccessor(match, curfile, confirmmatch(file1, file2),
              ordertype == ORDER_MTIME ? sort_pairs_by_mtime :
              ordertype == ORDER_CTIME ? sort_pairs_by_ctime :
                                         sort_pairs_by_filename, loginfo );
      }
      else if (ISFLAG(flags, F_DEFERCONFIRMATION) || confirmmatch(file1, file2))
        registerpair(match, curfile,
            ordertype == ORDER_MTIME ? sort_pairs_by_mtime :
            ordertype == ORDER_CTIME ? sort_pairs_by_ctime :
                                       sort_pairs_by_filename );

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

  if (loginfo != 0)
  {
    log_close(loginfo);
    loginfo = 0;
  }

  if (ISFLAG(flags, F_DELETEFILES))
  {
    if (ISFLAG(flags, F_NOPROMPT) || ISFLAG(flags, F_IMMEDIATE))
    {
      deletefiles(files, 0, 0, logfile);
    }
    else
    {
#ifndef NO_NCURSES
      if (!ISFLAG(flags, F_PLAINPROMPT))
      {
        if (newterm(getenv("TERM"), stdout, stdin) != 0)
        {
          deletefiles_ncurses(files, logfile);
        }
        else
        {
          errormsg("could not enter screen mode; falling back to plain mode\n\n");
          SETFLAG(flags, F_PLAINPROMPT);
        }
      }

      if (ISFLAG(flags, F_PLAINPROMPT))
      {
        if (freopen("/dev/tty", "r", stdin) == NULL)
        {
          errormsg("could not open terminal for input\n");
          exit(1);
        }

        deletefiles(files, 1, stdin, logfile);
      }
#else
      if (freopen("/dev/tty", "r", stdin) == NULL)
      {
        errormsg("could not open terminal for input\n");
        exit(1);
      }

      deletefiles(files, 1, stdin, logfile);
#endif
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
