/* FDUPES Copyright (c) 1999 Adrian Lopez

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
#include <getopt.h>
#include <string.h>
#include <errno.h>

#ifndef EXTERNAL_MD5
#include "md5/md5.h"
#endif

#define ISFLAG(a,b) ((a & b) == b)
#define SETFLAG(a,b) (a |= b)

#define F_RECURSE           0x001
#define F_HIDEPROGRESS      0x002
#define F_DSAMELINE         0x004
#define F_FOLLOWLINKS       0x008
#define F_DELETEFILES       0x010
#define F_EXCLUDEEMPTY      0x020
#define F_CONSIDERHARDLINKS 0x040
#define F_SHOWSIZE          0x080
#define F_OMITFIRST         0x100

char *program_name;

unsigned long flags = 0;

#define CHUNK_SIZE 8192

#define INPUT_SIZE 256

typedef struct _file {
  char *d_name;
  off_t size;
  char *crcsignature;
  ino_t inode;
  int hasdupes; /* true only if file is first on duplicate chain */
  struct _file *duplicates;
  struct _file *next;
} file_t;

typedef struct _filetree {
  file_t *file; 
#ifdef EXPERIMENTAL_RBTREE
  unsigned char color;
  struct _filetree *parent;
#endif
  struct _filetree *left;
  struct _filetree *right;
} filetree_t;

#ifdef EXPERIMENTAL_RBTREE
#define COLOR_RED    0
#define COLOR_BLACK  1
#endif

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

ino_t getinode(char *filename) {
  struct stat s;
   
  if (stat(filename, &s) != 0) return 0;

  return s.st_ino;   
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

      newfile->inode = 0;
      newfile->crcsignature = NULL;
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
char *getcrcsignature(char *filename)
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

  file = fopen(filename, "rb");
  if (file == NULL) {
    errormsg("error opening file %s\n", filename);
    return NULL;
  }
 
  while (fsize > 0) {
    toread = (fsize % CHUNK_SIZE) ? (fsize % CHUNK_SIZE) : CHUNK_SIZE;
    if (fread(chunk, toread, 1, file) != 1) {
      errormsg("error reading from file %s\n", filename);
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

#ifdef EXPERIMENTAL_RBTREE
/* Use a red-black tree structure to store file information.
 */

void rotate_left(filetree_t **root, filetree_t *node)
{
  filetree_t *subject;

  subject = node->right;
  node->right = subject->left;

  if (subject->left != NULL) subject->left->parent = node;
  subject->parent = node->parent;

  if (node->parent == NULL) {
    *root = subject;
  } else {
    if (node == node->parent->left)
      node->parent->left = subject;
    else 
      node->parent->right = subject;
  }

  subject->left = node;
  node->parent = subject;
}

void rotate_right(filetree_t **root, filetree_t *node)
{
  filetree_t *subject;

  subject = node->left;
  node->left = subject->right;

  if (subject->right != NULL) subject->right->parent = node;
  subject->parent = node->parent;

  if (node->parent == NULL) {
    *root = subject;
  } else {
    if (node == node->parent->left)
      node->parent->left = subject;
    else 
      node->parent->right = subject;
  }

  subject->right = node;
  node->parent = subject;
}

#define TREE_LEFT -1
#define TREE_RIGHT 1
#define TREE_ROOT  0

void registerfile(filetree_t **root, filetree_t *parent, int loc, file_t *file)
{
  filetree_t *node;
  filetree_t *uncle;

  file->size = filesize(file->d_name);
  file->inode = getinode(file->d_name);

  node = (filetree_t*) malloc(sizeof(filetree_t));
  if (node == NULL) {
    errormsg("out of memory!\n");
    exit(1);
  }
  
  node->file = file;
  node->left = NULL;
  node->right = NULL;
  node->parent = parent;
  node->color = COLOR_RED;

  if (loc == TREE_ROOT)
    *root = node;
  else if (loc == TREE_LEFT) 
    parent->left = node;
  else 
    parent->right = node;

  while (node != *root && node->parent->color == COLOR_RED) {
    if (node->parent->parent == NULL) return;

    if (node->parent == node->parent->parent->left) {
      uncle = node->parent->parent->right;
      if (uncle == NULL) return;

      if (uncle->color == COLOR_RED) {
	node->parent->color = COLOR_BLACK;
	uncle->color = COLOR_BLACK;
	node->parent->parent->color = COLOR_RED;
	node = node->parent->parent;
      } else {
	if (node == node->parent->right) {
	  node = node->parent;
	  rotate_left(root, node);
	}
	node->parent->color = COLOR_BLACK;
	node->parent->parent->color = COLOR_RED;
	rotate_right(root, node->parent->parent);
      }
    } else {
      uncle = node->parent->parent->left;
      if (uncle == NULL) return;

      if (uncle->color == COLOR_RED) {
	node->parent->color = COLOR_BLACK;
	uncle->color = COLOR_BLACK;
	node->parent->parent->color = COLOR_RED;
	node = node->parent->parent;
      } else {
	if (node == node->parent->right) {
	  node = node->parent;
	  rotate_left(root, node);
	}
	node->parent->color = COLOR_BLACK;
	node->parent->parent->color = COLOR_RED;
	rotate_right(root, node->parent->parent);
      }
    }
  }

  (*root)->color = COLOR_BLACK;
}

#endif /* [#ifdef EXPERIMENTAL_RBTREE] */

#ifndef EXPERIMENTAL_RBTREE

int registerfile(filetree_t **branch, file_t *file)
{
  file->size = filesize(file->d_name);
  file->inode = getinode(file->d_name);

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

#endif /* [#ifndef EXPERIMENTAL_RBTREE] */

file_t *checkmatch(filetree_t **root, filetree_t *checktree, file_t *file)
{
  int cmpresult;
  char *crcsignature;
  off_t fsize;

  /* If inodes are equal one of the files is a hard link, which
     is usually not accidental. We don't want to flag them as 
     duplicates, unless the user specifies otherwise. */

  if (!ISFLAG(flags, F_CONSIDERHARDLINKS) && getinode(file->d_name) == 
   checktree->file->inode) return NULL;

  fsize = filesize(file->d_name);
  
  if (fsize < checktree->file->size) 
    cmpresult = -1;
  else 
    if (fsize > checktree->file->size) cmpresult = 1;
  else {
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
  }

  if (cmpresult < 0) {
    if (checktree->left != NULL) {
      return checkmatch(root, checktree->left, file);
    } else {
#ifndef EXPERIMENTAL_RBTREE
      registerfile(&(checktree->left), file);
#else
      registerfile(root, checktree, TREE_LEFT, file);
#endif
      return NULL;
    }
  } else if (cmpresult > 0) {
    if (checktree->right != NULL) {
      return checkmatch(root, checktree->right, file);
    } else {
#ifndef EXPERIMENTAL_RBTREE
      registerfile(&(checktree->right), file);
#else
      registerfile(root, checktree, TREE_RIGHT, file);
#endif
      return NULL;
    }
  } else return checktree->file;
}

/* Do a bit-for-bit comparison in case two different files produce the 
   same signature. Unlikely, but better safe than sorry. */

int confirmmatch(FILE *file1, FILE *file2)
{
  unsigned char c1;
  unsigned char c2;
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

void printmatches(file_t *files)
{
  file_t *tmpfile;

  while (files != NULL) {
    if (files->hasdupes) {
      if (!ISFLAG(flags, F_OMITFIRST)) {
	if (ISFLAG(flags, F_SHOWSIZE)) printf("%ld byte%seach:\n", files->size,
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

void autodelete(file_t *files)
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

      printf("[%d] %s\n", counter, files->d_name);

      tmpfile = files->duplicates;

      while (tmpfile) {
	dupelist[++counter] = tmpfile;
	printf("[%d] %s\n", counter, tmpfile->d_name);
	tmpfile = tmpfile->duplicates;
      }

      printf("\n");

      do {
	printf("Set %d of %d, preserve files [1 - %d, all]", 
          curgroup, groups, counter);
	if (ISFLAG(flags, F_SHOWSIZE)) printf(" (%ld byte%seach)", files->size,
	  (files->size != 1) ? "s " : " ");
	printf(": ");
	fflush(stdout);

	fgets(preservestr, INPUT_SIZE, stdin);

	i = strlen(preservestr) - 1;

	while (preservestr[i]!='\n'){ /* tail of buffer must be a newline */
	  tstr = (char*)
	    realloc(preservestr, strlen(preservestr) + 1 + INPUT_SIZE);
	  if (!tstr) { /* couldn't allocate memory, treat as fatal */
	    errormsg("out of memory!\n");
	    exit(1);
	  }

	  preservestr = tstr;
	  if (!fgets(preservestr + i + 1, INPUT_SIZE, stdin))
	    break; /* stop if fgets fails -- possible EOF? */
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
	  printf("   [-] %s\n", dupelist[x]->d_name);
	  remove(dupelist[x]->d_name);
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

void help_text()
{
  printf("Usage: fdupes [options] DIRECTORY...\n\n");

  printf(" -r --recurse     \tinclude files residing in subdirectories\n");
  printf(" -s --symlinks    \tfollow symlinks\n");
  printf(" -H --hardlinks   \tnormally, when two or more files point to the same\n");
  printf("                  \tdisk area they are treated as non-duplicates; this\n"); 
  printf("                  \toption will change this behavior\n");
  printf(" -n --noempty     \texclude zero-length files from consideration\n");
  printf(" -f --omitfirst   \tomit the first file in each set of matches\n");
  printf(" -1 --sameline    \tlist each set of matches on a single line\n");
  printf(" -S --size        \tshow size of duplicate files\n");
  printf(" -q --quiet       \thide progress indicator\n");
  printf(" -d --delete      \tprompt user for files to preserve and delete all\n"); 
  printf("                  \tothers; important: under particular circumstances,\n");
  printf("                  \tdata may be lost when using this option together\n");
  printf("                  \twith -s or --symlinks, or when specifying a\n");
  printf("                  \tparticular directory more than once; refer to the\n");
  printf("                  \tfdupes documentation for additional information\n");
  printf(" -v --version     \tdisplay fdupes version\n");
  printf(" -h --help        \tdisplay this help message\n\n");
}

int main(int argc, char **argv) {
  int x;
  int opt;
  FILE *file1;
  FILE *file2;
  file_t *files = NULL;
  file_t *curfile;
  file_t *match = NULL;
  filetree_t *checktree = NULL;
  int filecount = 0;
  int progress = 0;
 
  static struct option long_options[] = 
  {
    { "omitfirst", 0, 0, 'f' },
    { "recurse", 0, 0, 'r' },
    { "quiet", 0, 0, 'q' },
    { "sameline", 0, 0, '1' },
    { "size", 0, 0, 'S' },
    { "symlinks", 0, 0, 's' },
    { "hardlinks", 0, 0, 'H' },
    { "noempty", 0, 0, 'n' },
    { "delete", 0, 0, 'd' },
    { "version", 0, 0, 'v' },
    { "help", 0, 0, 'h' },
    { 0, 0, 0, 0 }
  };

  program_name = argv[0];

  while ((opt = getopt_long(argc, argv, "frq1SsHndvh", long_options, NULL)) != EOF) {
    switch (opt) {
    case 'f':
      SETFLAG(flags, F_OMITFIRST);
      break;
    case 'r':
      SETFLAG(flags, F_RECURSE);
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
    default:
      fprintf(stderr, "Try `fdupes --help' for more information\n");
      exit(1);
    }
  }

  if (optind >= argc) {
    errormsg("no directories specified\n");
    exit(1);
  }

  for (x = optind; x < argc; x++) filecount += grokdir(argv[x], &files);

  if (!files) exit(0);
  
  curfile = files;

  while (curfile) {
    if (!checktree) 
#ifndef EXPERIMENTAL_RBTREE
      registerfile(&checktree, curfile);
#else
      registerfile(&checktree, NULL, TREE_ROOT, curfile);
#endif
    else 
      match = checkmatch(&checktree, checktree, curfile);

    if (match != NULL) {
      file1 = fopen(curfile->d_name, "rb");
      if (!file1) {
	curfile = curfile->next;
	continue;
      }

      file2 = fopen(match->d_name, "rb");
      if (!file2) {
	fclose(file1);
	curfile = curfile->next;
	continue;
      }
 
      if (confirmmatch(file1, file2)) {
	match->hasdupes = 1;
        curfile->duplicates = match->duplicates;
        match->duplicates = curfile;
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

  if (ISFLAG(flags, F_DELETEFILES)) autodelete(files); 
  else printmatches(files);

  while (files) {
    curfile = files->next;
    free(files->d_name);
    free(files);
    files = curfile;
  }
  
  purgetree(checktree);

  return 0;
}
