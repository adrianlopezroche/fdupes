/* FDUPES Copyright (c) 2022 Adrian Lopez

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

#ifndef HASHDB_H
#define HASHDB_H

#include "fdupes.h"
#include <sqlite3.h>

sqlite3 *hashdb_open(const char *path);
int hashdb_close(sqlite3 *db);
int hashdb_begintransaction(sqlite3 *db);
int hashdb_committransaction(sqlite3 *db);
int hashdb_rollbacktransaction(sqlite3 *db);
int hashdb_vacuum(sqlite3 *db);
int hashdb_getdirectoryid(sqlite3 *db, const char *path, sqlite3_int64 *directoryid);
int hashdb_savedirectory(sqlite3 *db, const char *path);
int hashdb_deletedirectory(sqlite3 *db, sqlite3_int64 id);
int hashdb_cleardirectories(sqlite3 *db);
int hashdb_foreachdirectory(sqlite3 *db, const sqlite3_int64 *parentid, int (*callback)(const sqlite3_int64, const char*, const char*, const sqlite3_int64));
int hashdb_loadhash(sqlite3 *db, const file_t *entry, md5_byte_t **partialhash, md5_byte_t **fullhash);
int hashdb_savehash(sqlite3 *db, const file_t *entry, md5_byte_t *partialhash, md5_byte_t *fullhash);
int hashdb_foreachhash(sqlite3 *db, sqlite3_int64 *directoryid, int (*callback)(const sqlite3_int64, const char*, const char*));
int hashdb_deletehash(sqlite3 *db, sqlite3_int64 directoryid, const char *filename);
int hashdb_deletehashforpath(sqlite3 *db, const char *path);

#endif