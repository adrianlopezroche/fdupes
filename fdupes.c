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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#define ISFLAG(a,b) ((a & b) == b)
#define SETFLAG(a,b) (a |= b)

#define F_RECURSE         0x01
#define F_HIDEPROGRESS    0x02
#define F_DSAMELINE       0x04
#define F_FOLLOWLINKS     0x08
#define F_DELETEFILES     0x10

unsigned long flags = 0;

#define SIGNATURE_FILE "/tmp/fdupes.md5sum"

typedef struct _file {
  char *d_name;
  off_t size;
  char *crcsignature;
  int hasdupes; /* true only if file is first on duplicate chain */
  struct _file *duplicates;
  struct _file *next;
} file_t;

typedef struct _filetree {
  file_t *file;
  struct _filetree *left;
  struct _filetree *right;
} filetree_t;

off_t filesize(char *filename) {
  struct stat s;

  if (stat(filename, &s) != 0) return -1;

  return s.st_size;
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

  cd = opendir(dir);

  if (!cd) {
    fprintf(stderr, "%s not a directory\n", dir);
    return 0;
  }

  while ((dirinfo = readdir(cd)) != NULL) {
    if (strcmp(dirinfo->d_name, ".") && strcmp(dirinfo->d_name, "..")) {
      newfile = (file_t*) malloc(sizeof(file_t));

      if (!newfile) {
	fprintf(stderr, "Out of memory!\n");
	closedir(cd);
	return filecount;
      } else newfile->next = *filelistp;

      newfile->crcsignature = NULL;
      newfile->duplicates = NULL;
      newfile->hasdupes = 0;

      newfile->d_name = (char*)malloc(strlen(dir)+strlen(dirinfo->d_name)+2);

      if (!newfile->d_name) {
	fprintf(stderr, "Out of memory!\n");
	free(newfile);
	closedir(cd);
	return filecount;
      }

      strcpy(newfile->d_name, dir);
      lastchar = strlen(dir) - 1;
      if (lastchar >= 0 && dir[lastchar] != '/')
	strcat(newfile->d_name, "/");
      strcat(newfile->d_name, dirinfo->d_name);
      
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
	if (ISFLAG(flags, F_RECURSE) && (ISFLAG(flags, F_FOLLOWLINKS) || !S_ISLNK(linfo.st_mode))) filecount += grokdir(newfile->d_name, filelistp);
	  free(newfile->d_name);
	  free(newfile);
      } else {
	if (ISFLAG(flags, F_FOLLOWLINKS) || !S_ISLNK(linfo.st_mode)) {
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

char *getcrcsignature(char *filename)
{
  static char signature[256];
  char *separator;
  FILE *result;
  int pid;

  pid = fork();

  if (pid == 0) {
    freopen(SIGNATURE_FILE, "w", stdout);
    execlp("md5sum", "md5sum", filename, NULL);
    fclose(stdout);
  } else if (pid != -1) {
    waitpid(pid, NULL, 0);
  } else {
    return NULL;
  }

  result = fopen(SIGNATURE_FILE, "r");
  if (!result) return NULL;

  fgets(signature, 256, result);
  separator = strchr(signature, ' ');
  if (separator) *separator = '\0';

  fclose(result);

  return signature;
}

void purgetree(filetree_t *checktree)
{
  if (checktree->left != NULL) purgetree(checktree->left);
    
  if (checktree->right != NULL) purgetree(checktree->right);
    
  free(checktree);
}

int registerfile(filetree_t **branch, file_t *file)
{
  file->size = filesize(file->d_name);

  *branch = (filetree_t*) malloc(sizeof(filetree_t));
  if (*branch == NULL) return 0;
  
  (*branch)->file = file;
  (*branch)->left = NULL;
  (*branch)->right = NULL;

  return 1;
}

file_t *checkmatch(filetree_t *checktree, file_t *file)
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
    if (checktree->file->crcsignature == NULL) {
      crcsignature = getcrcsignature(checktree->file->d_name);

      checktree->file->crcsignature = (char*) malloc(strlen(crcsignature)+1);
      if (checktree->file->crcsignature == NULL) return 0;
      strcpy(checktree->file->crcsignature, crcsignature);
    }

    if (file->crcsignature == NULL) {
      crcsignature = getcrcsignature(file->d_name);
      
      file->crcsignature = (char*) malloc(strlen(crcsignature)+1);
      if (file->crcsignature == NULL) return 0;
      strcpy(file->crcsignature, crcsignature);
    }

    cmpresult = strcmp(file->crcsignature, checktree->file->crcsignature);
  }

  if (cmpresult < 0) {
    if (checktree->left != NULL) {
      return checkmatch(checktree->left, file);
    } else {
      registerfile(&(checktree->left), file);
      return NULL;
    }
  } else if (cmpresult > 0) {
    if (checktree->right != NULL) {
      return checkmatch(checktree->right, file);
    } else {
      registerfile(&(checktree->right), file);
      return NULL;
    }
  } else return checktree->file;
}

/* Just in case two different files produce the same signature. Extremely
   unlikely, to be sure, but better safe than sorry. */

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
      printf("%s%c", files->d_name, ISFLAG(flags, F_DSAMELINE)?' ':'\n');
      tmpfile = files->duplicates;
      while (tmpfile != NULL) {
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
  int counter = 1;
  int groups = 0;
  int curgroup = 0;
  file_t *tmpfile;
  file_t *curfile;
  file_t **dupelist;
  int *preserve;
  char *preservestr;
  char *token;
  int number;
  int sum;
  int max = 0;
  int x;

  curfile = files;
  
  while (curfile) {
    if (curfile->hasdupes) {
      groups++;

      counter = 1;
      tmpfile = curfile->duplicates;
      while (tmpfile) {
	counter++;
	tmpfile = tmpfile->duplicates;
      }
      
      if ((counter + 1) > max) max = counter + 1;
    }
    
    curfile = curfile->next;
  }
  
  dupelist = (file_t**) malloc(sizeof(file_t*) * max);
  preserve = (int*) malloc(sizeof(int) * max);
  preservestr = (char*) malloc(sizeof(char) * max * 256);

  while (files) {
    if (files->hasdupes) {
      curgroup++;
      counter = 1;
      dupelist[counter] = files;
      printf("[%d] %s\n", counter++, files->d_name);
      tmpfile = files->duplicates;
      while (tmpfile) {
	dupelist[counter] = tmpfile;
	printf("[%d] %s\n", counter++, tmpfile->d_name);
	tmpfile = tmpfile->duplicates;
      }

      printf("\n");

      do {
	printf("Preserve files [%d/%d]: ", curgroup, groups);
	fflush(stdout);
	fgets(preservestr, max * 256, stdin);

       	if (strcasecmp(preservestr, "all\n") == 0) {
	  for (x = 1; x < counter; x++) preserve[x] = 1;
	} else {
	  for (x = 1; x < counter; x++) preserve[x] = 0;
	  
	  token = strtok(preservestr, " ,");

	  while (token != NULL) {
	    number = 0;
	    sscanf(token, "%d", &number);
	    if (number > 0 && number < counter) preserve[number] = 1;
	    token = strtok(NULL, " ,");
	  }
	}
      
	for (sum = 0, x = 1; x < counter; x++) sum += preserve[x];
      } while (sum < 1);

      printf("\n");
      for (x = 1; x < counter; x++) { 
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

  printf(" -r --recurse\tinclude files residing in subdirectories\n");
  printf(" -q --quiet  \thide progress indicator\n");
  printf(" -1 --sameline\tlist duplicates on a single line\n");
  printf(" -s --symlinks\tfollow symlinks\n");
  printf(" -d --delete  \tprompt user for files to preserve and delete all others\n"); 
  printf("              \timportant: using this option together with -s or --symlinks\n");
  printf("              \tor when specifiying the same directory more than once\n");
  printf("              \tcan lead to accidental data loss\n");
  printf(" -v --version \tdisplay fdupes version\n");
  printf(" -h --help   \tdisplay this help message\n\n");
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
    { "recurse", 0, 0, 'r' },
    { "quiet", 0, 0, 'q' },
    { "sameline", 0, 0, '1' },
    { "symlinks", 0, 0, 's' },
    { "delete", 0, 0, 'd' },
    { "help", 0, 0, 'h' },
    { "version", 0, 0, 'v' },
    { 0, 0, 0, 0 }
  };

  while ((opt = getopt_long(argc, argv, "rq1sdvh", long_options, NULL)) != EOF) {
    switch (opt) {
    case 'r':
      SETFLAG(flags, F_RECURSE);
      break;
    case 'q':
      SETFLAG(flags, F_HIDEPROGRESS);
      break;
    case '1':
      SETFLAG(flags, F_DSAMELINE);
      break;
    case 's':
      SETFLAG(flags, F_FOLLOWLINKS);
      break;
    case 'd':
      SETFLAG(flags, F_DELETEFILES);
      break;
    case 'v':
      printf("FDUPES version %s\n", VERSION);
      exit(0);
    case 'h':
      help_text();
      exit(1);
    default:
      printf("Try `fdupes --help' for more information\n");
      exit(1);
    }
  }

  if (optind >= argc) {
    printf("%s: no directories specified\n", argv[0]);
    exit(1);
  }

  for (x = optind; x < argc; x++) filecount += grokdir(argv[x], &files);

  if (!files) {
    printf("%s: no files found\n", argv[0]);
    exit(0);
  }
  
  curfile = files;

  while (curfile) {
    if (!checktree) 
      registerfile(&checktree, curfile);
    else 
      match = checkmatch(checktree, curfile);

    if (match != NULL) {
      file1 = fopen(curfile->d_name, "rb");
      if (!file1) continue;

      file2 = fopen(match->d_name, "rb");
      if (!file2) {
	fclose(file1);
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
      fprintf(stderr, "\rProgress [%d/%d] %d%%", progress, filecount,
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

  remove(SIGNATURE_FILE);

  return 0;
}
