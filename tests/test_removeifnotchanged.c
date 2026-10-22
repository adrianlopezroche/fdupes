/* Synthetic-only regression tests: stat and remove are replaced with mocks. */
#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int mock_stat(const char *path, struct stat *st);
static int mock_remove(const char *path);

#define stat(path, st) mock_stat(path, st)
#define remove(path) mock_remove(path)
#include "../removeifnotchanged.c"
#undef stat
#undef remove

static struct stat stat_snapshot;
static int stat_fails;
static int remove_calls;
static const char *removed_path;

static int mock_stat(const char *path, struct stat *st)
{
    (void)path;
    /* Failed stat results are invalid even if an implementation left matching bytes. */
    *st = stat_snapshot;
    if (stat_fails) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static int mock_remove(const char *path)
{
    ++remove_calls;
    removed_path = path;
    return 0;
}

static void prepare_snapshot(file_t *file)
{
    memset(&stat_snapshot, 0, sizeof(stat_snapshot));
    stat_snapshot.st_dev = 7;
    stat_snapshot.st_ino = 19;
    stat_snapshot.st_ctime = 31;
    stat_snapshot.st_mtime = 37;
    stat_snapshot.st_size = 41;

    memset(file, 0, sizeof(*file));
    file->d_name = (char *)"synthetic/duplicate";
    file->device = stat_snapshot.st_dev;
    file->inode = stat_snapshot.st_ino;
    file->ctime = stat_snapshot.st_ctime;
    file->mtime = stat_snapshot.st_mtime;
    file->size = stat_snapshot.st_size;
}

int main(void)
{
    file_t file;
    char *errorstring = NULL;
    int result;

    prepare_snapshot(&file);
    stat_fails = 1;
    result = removeifnotchanged(&file, &errorstring);
    if (result == 0 || remove_calls != 0 || errorstring == NULL) {
        fprintf(stderr, "FAIL: stat failure must not call remove (result=%d, calls=%d)\n",
                result, remove_calls);
        return 1;
    }

    prepare_snapshot(&file);
    stat_fails = 0;
    file.size++;
    errorstring = NULL;
    result = removeifnotchanged(&file, &errorstring);
    if (result != -2 || remove_calls != 0 || errorstring == NULL) {
        fprintf(stderr, "FAIL: changed metadata must not call remove\n");
        return 1;
    }

    prepare_snapshot(&file);
    stat_fails = 0;
    errorstring = NULL;
    result = removeifnotchanged(&file, &errorstring);
    if (result != 0 || remove_calls != 1 ||
        strcmp(removed_path, "synthetic/duplicate") != 0) {
        fprintf(stderr, "FAIL: unchanged metadata should record one mock removal\n");
        return 1;
    }

    puts("3 synthetic mock-filesystem cases passed; no filesystem mutation performed");
    return 0;
}
