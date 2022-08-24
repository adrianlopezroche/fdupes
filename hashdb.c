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

#include "config.h"
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "hashdb.h"
#include "getrealpath.h"
#include "sbasename.h"
#include "sdirname.h"
#include "errormsg.h"

#define DATABASE_VERSION 1

#define HASH_FUNCTION_MD5 1

#define HASH_FUNCTION HASH_FUNCTION_MD5
#define HASH_FUNCTION_OUTPUT_LENGTH 16

void md5copy(md5_byte_t *to, const md5_byte_t *from);

#define PREPARE_STATEMENT(a, b) sqlite3_prepare_v2(db, a, -1, hashdb__newstatement(&b), 0)

#define HASHDB_MAX_STATEMENTS 32

sqlite3_stmt **hashdb_statements[HASHDB_MAX_STATEMENTS];

size_t hashdb_statements_top;

sqlite3_stmt *query_begintransaction = 0;
sqlite3_stmt *query_committransaction = 0;
sqlite3_stmt *query_rollbacktransaction = 0;
sqlite3_stmt *query_vacuum = 0;
sqlite3_stmt *query_getdirectoryid = 0;
sqlite3_stmt *query_insertdirectory = 0;
sqlite3_stmt *query_deletedirectory = 0;
sqlite3_stmt *query_cleardirectories = 0;
sqlite3_stmt *query_foreachdirectory = 0;
sqlite3_stmt *query_foreachdirectorywithin = 0;
sqlite3_stmt *query_loadhash = 0;
sqlite3_stmt *query_savehash = 0;
sqlite3_stmt *query_deletehash = 0;
sqlite3_stmt *query_deletehashforpath = 0;
sqlite3_stmt *query_foreachhash = 0;
sqlite3_stmt *query_foreachhashwithin = 0;

sqlite3_stmt **hashdb__newstatement(sqlite3_stmt **statement)
{
  assert(hashdb_statements_top + 1 <= HASHDB_MAX_STATEMENTS);

  hashdb_statements[hashdb_statements_top++] = statement;

  return statement;
}

int hashdb__createtables(sqlite3 *db)
{
  int result;

  hashdb_begintransaction(db);

  result = sqlite3_exec(db,
    "CREATE TABLE IF NOT EXISTS directories ("
    "  id INTEGER PRIMARY KEY,"
    "  name TEXT,"
    "  full_path TEXT UNIQUE,"
    "  parent INTEGER REFERENCES directories(id) ON DELETE CASCADE"
    ")",
    0, 0, 0);

  if (result != SQLITE_OK)
    return result;

  result = sqlite3_exec(db,
    "CREATE TABLE IF NOT EXISTS hashes ("
    "  directory_id INTEGER REFERENCES directories(id) ON DELETE CASCADE,"
    "  filename TEXT,"
    "  inode BLOB,"
    "  size INTEGER,"
    "  ctime BLOB,"
    "  mtime BLOB,"
    "  ctime_nsec INTEGER,"
    "  mtime_nsec INTEGER,"
    "  partial_hash BLOB,"
    "  partial_hash_bytes INTEGER,"
    "  hash BLOB,"
    "  hash_function INTEGER,"
    "  PRIMARY KEY (directory_id, filename)"
    ")",
    0, 0, 0);

  if (result != SQLITE_OK) {
    hashdb_rollbacktransaction(db);
    return result;
  }

  hashdb_committransaction(db);

  return SQLITE_OK;
}

int hashdb__preparestatements(sqlite3 *db)
{
  int result;

  /* standard SQL commands */
  result = PREPARE_STATEMENT("BEGIN", query_begintransaction);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("COMMIT", query_committransaction);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("ROLLBACK", query_rollbacktransaction);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("VACUUM", query_vacuum);
  if (result != SQLITE_OK)
    return result;

  /* directory operations */
  result = PREPARE_STATEMENT("SELECT id FROM directories WHERE full_path = ?", query_getdirectoryid);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("INSERT INTO directories (name, full_path, parent) VALUES (?, ?, ?)", query_insertdirectory);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("DELETE FROM directories WHERE id = ?", query_deletedirectory);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("DELETE FROM directories", query_cleardirectories);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("SELECT id, name, full_path, parent FROM directories", query_foreachdirectory);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("SELECT id, name, full_path, parent FROM directories WHERE parent = :parent", query_foreachdirectorywithin);
  if (result != SQLITE_OK)
    return result;

  /* hash operations */
  result = PREPARE_STATEMENT("SELECT hashes.partial_hash, hashes.hash FROM hashes INNER JOIN directories ON hashes.directory_id = directories.id WHERE directories.full_path = ? AND hashes.filename = ? AND hashes.inode = ? AND hashes.size = ? AND hashes.ctime = ? AND hashes.mtime = ? AND hashes.ctime_nsec = ? AND hashes.mtime_nsec = ? AND hashes.partial_hash_bytes = ? AND hashes.hash_function = ?", query_loadhash);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("INSERT OR REPLACE INTO hashes (directory_id, filename, inode, size, ctime, mtime, ctime_nsec, mtime_nsec, partial_hash, partial_hash_bytes, hash, hash_function) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", query_savehash);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("DELETE FROM hashes WHERE directory_id = ? AND filename = ?", query_deletehash);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("DELETE FROM hashes WHERE filename = ? AND directory_id IN (SELECT id FROM directories WHERE full_path = ?)", query_deletehashforpath);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("SELECT hashes.directory_id, hashes.filename, directories.full_path AS directory FROM hashes INNER JOIN directories ON hashes.directory_id = directories.id", query_foreachhash);
  if (result != SQLITE_OK)
    return result;

  result = PREPARE_STATEMENT("SELECT hashes.directory_id, hashes.filename, directories.full_path AS directory FROM hashes INNER JOIN directories ON hashes.directory_id = directories.id WHERE directories.id = :directory_id", query_foreachhashwithin);
  if (result != SQLITE_OK)
    return result;

  return SQLITE_OK;
}

void hashdb__finalizestatements()
{
  size_t s;

  for (s = 0; s < hashdb_statements_top; ++s) {
    sqlite3_finalize(*hashdb_statements[s]);
    *hashdb_statements[s] = 0;
  }

  hashdb_statements_top = 0;
}

int hashdb__getdatabaseversion(sqlite3 *db, int *version)
{
  sqlite3_stmt *statement;
  int result;

  result = sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &statement, 0);
  if (result != SQLITE_OK)
    return result;

  result = sqlite3_step(statement);
  if (result != SQLITE_ROW)
    return result;

  *version = sqlite3_column_int(statement, 0);

  result = sqlite3_finalize(statement);
  if (result != SQLITE_OK)
    return result;

  return SQLITE_OK;
}

int hashdb__setdatabaseversion(sqlite3 *db, int version)
{
  char query[64];
  int written;

  written = snprintf(query, sizeof(query), "PRAGMA user_version = %d", version);
  if (written >= sizeof(query))
      return SQLITE_ERROR;

  return sqlite3_exec(db, query, 0, 0, 0);
}

int hashdb__iscompatible(int major)
{
  return major <= DATABASE_VERSION;
}

int hashdb__insertdirectory(sqlite3 *db, const char *name, const char *full_path, const sqlite3_int64 *parent)
{
  int result;

  sqlite3_bind_text(query_insertdirectory, 1, name, strlen(name), SQLITE_TRANSIENT);
  sqlite3_bind_text(query_insertdirectory, 2, full_path, strlen(full_path), SQLITE_TRANSIENT);

  if (parent != 0)
    sqlite3_bind_int64(query_insertdirectory, 3, *parent);
  else
    sqlite3_bind_null(query_insertdirectory, 3);

  result = sqlite3_step(query_insertdirectory);

  sqlite3_reset(query_insertdirectory);

  return result == SQLITE_DONE;
}

int hashdb__enable_foreign_keys(sqlite3 *db)
{
  return sqlite3_exec(db, "PRAGMA foreign_keys = ON", 0, 0, 0) == SQLITE_OK;
}

int hashdb__foreign_keys_enabled(sqlite3 *db)
{
  sqlite3_stmt *statement;
  int value;
  int result;

  result = sqlite3_prepare_v2(db, "PRAGMA foreign_keys", -1, &statement, 0);
  if (result != SQLITE_OK)
    return 0;

  result = sqlite3_step(statement);
  if (result != SQLITE_ROW)
    return 0;

  value = sqlite3_column_int(statement, 0);

  result = sqlite3_finalize(statement);
  if (result != SQLITE_OK)
    return 0;

  return value;
}

int hashdb__enable_write_ahead(sqlite3 *db)
{
  return sqlite3_exec(db, "PRAGMA journal_mode = WAL", 0, 0, 0) == SQLITE_OK;
}

sqlite3 *hashdb_open(const char *path)
{
  sqlite3 *db;
  int result;
  int version;

  result = sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
  if (result != SQLITE_OK)
    return 0;

  if (!hashdb__enable_write_ahead(db)) {
    sqlite3_close_v2(db);
    return 0;
  }

  if (!hashdb__enable_foreign_keys(db)) {
    sqlite3_close_v2(db);
    return 0;
  }

  if (!hashdb__foreign_keys_enabled(db)) {
    sqlite3_close_v2(db);
    return 0;
  }

  result = hashdb__getdatabaseversion(db, &version);
  if (result != SQLITE_OK || !hashdb__iscompatible(version)) {
    sqlite3_close_v2(db);
    return 0;
  }

  if (version == 0) /* this is a new database */ {
    result = hashdb__createtables(db);
    if (result != SQLITE_OK) {
      sqlite3_close_v2(db);
      return 0;
    }

    result = hashdb__setdatabaseversion(db, DATABASE_VERSION);
    if (result != SQLITE_OK) {
      sqlite3_close_v2(db);
      return 0;
    }
  }

  if (hashdb__preparestatements(db) != SQLITE_OK) {
    sqlite3_close_v2(db);
    return 0;
  }

  return db;
}

int hashdb_close(sqlite3 *db)
{
  hashdb__finalizestatements();

  return sqlite3_close_v2(db);
}

int hashdb_begintransaction(sqlite3 *db)
{
  int result;

  result = sqlite3_step(query_begintransaction);

  sqlite3_reset(query_begintransaction);

  return result == SQLITE_DONE;
}

int hashdb_committransaction(sqlite3 *db)
{
  int result;

  result = sqlite3_step(query_committransaction);

  sqlite3_reset(query_committransaction);

  return result == SQLITE_DONE;
}

int hashdb_rollbacktransaction(sqlite3 *db)
{
  int result;

  result = sqlite3_step(query_rollbacktransaction);

  sqlite3_reset(query_rollbacktransaction);

  return result == SQLITE_DONE;
}

int hashdb_vacuum(sqlite3 *db)
{
  int result;

  result = sqlite3_step(query_vacuum);

  sqlite3_reset(query_vacuum);

  return result == SQLITE_DONE;
}

int hashdb_getdirectoryid(sqlite3 *db, const char *path, sqlite_int64 *directory_id)
{
  int result;

  sqlite3_bind_text(query_getdirectoryid, 1, path, strlen(path), SQLITE_TRANSIENT);

  result = sqlite3_step(query_getdirectoryid);

  if (result == SQLITE_ROW)
    *directory_id = sqlite3_column_int64(query_getdirectoryid, 0);

  sqlite3_reset(query_getdirectoryid);

  return result == SQLITE_ROW;
}

int hashdb_savedirectory(sqlite3 *db, const char *path)
{
  int result;
  char *dir;
  char *base;
  sqlite3_int64 parentid;

  dir = sdirname(0, path);
  if (!dir)
    return 0;

  base = sbasename(0, path);
  if (!base)
  {
    free(dir);
    return 0;
  }

  if (!hashdb_getdirectoryid(db, dir, &parentid))
  {
    if (strcmp(path, "/") == 0)
    {
      result = hashdb__insertdirectory(db, "/", "/", 0);

      free(base);
      free(dir);

      return result;
    }

    if (!hashdb_savedirectory(db, dir))
    {
      free(base);
      free(dir);

      return 0;
    }

    parentid = sqlite3_last_insert_rowid(db);
  }

  result = hashdb__insertdirectory(db, base, path, &parentid);

  free(base);
  free(dir);

  return result;
}

int hashdb_deletedirectory(sqlite3 *db, sqlite3_int64 id)
{
  int result;

  sqlite3_bind_int64(query_deletedirectory, 1, id);

  result = sqlite3_step(query_deletedirectory);

  sqlite3_reset(query_deletedirectory);

  return result == SQLITE_DONE;
}

int hashdb_cleardirectories(sqlite3 *db)
{
  int result;

  result = sqlite3_step(query_cleardirectories);

  sqlite3_reset(query_cleardirectories);

  return result == SQLITE_DONE;
}

int hashdb_foreachdirectory(sqlite3 *db, const sqlite3_int64 *parent, int (*callback)(const sqlite3_int64, const char*, const char*, const sqlite3_int64))
{
  int result;
  sqlite3_stmt *query;

  if (parent != 0) {
    query = query_foreachdirectorywithin;
    sqlite3_bind_int64(query, 1, *parent);
  } else {
    query = query_foreachdirectory;
    sqlite3_bind_null(query, 1);
  }

  result = sqlite3_step(query);

  while (result == SQLITE_ROW)
  {
    result = callback(
      sqlite3_column_int64(query, 0),
      sqlite3_column_text(query, 1),
      sqlite3_column_text(query, 2),
      sqlite3_column_int64(query, 3)
    );

    if (result == 0) {
      sqlite3_reset(query);
      return 1;
    }

    result = sqlite3_step(query);
  }

  sqlite3_reset(query);

  return result == SQLITE_DONE;
}

int hashdb_loadhash(sqlite3 *db, const file_t *entry, md5_byte_t **partialhash, md5_byte_t **fullhash)
{
  int result;
  int hashsize;
  char *realpath;
  char *name;

  realpath = getrealpath(entry->d_name, 0);
  if (realpath == 0)
    return 0;

  name = malloc(strlen(realpath) + 1);
  if (name == 0)
  {
    free(realpath);
    return 0;
  }

  sdirname(name, realpath);
  sqlite3_bind_text(query_loadhash, 1, name, strlen(name), SQLITE_TRANSIENT);

  sbasename(name, realpath);
  sqlite3_bind_text(query_loadhash, 2, name, strlen(name), SQLITE_TRANSIENT);

  sqlite3_bind_blob(query_loadhash, 3, &entry->inode, sizeof(entry->inode), SQLITE_TRANSIENT);
  sqlite3_bind_int64(query_loadhash, 4, entry->size);
  sqlite3_bind_blob(query_loadhash, 5, &entry->ctime, sizeof(entry->ctime), SQLITE_TRANSIENT);
  sqlite3_bind_blob(query_loadhash, 6, &entry->mtime, sizeof(entry->mtime), SQLITE_TRANSIENT);
  sqlite3_bind_int64(query_loadhash, 7, entry->ctime_nsec);
  sqlite3_bind_int64(query_loadhash, 8, entry->mtime_nsec);
  sqlite3_bind_int64(query_loadhash, 9, PARTIAL_MD5_SIZE);
  sqlite3_bind_int(query_loadhash, 10, HASH_FUNCTION);

  result = sqlite3_step(query_loadhash);

  free(name);
  free(realpath);

  if (result != SQLITE_ROW)
  {
    sqlite3_reset(query_loadhash);
    return 0;
  }

  hashsize = sqlite3_column_bytes(query_loadhash, 0);

  if (hashsize == HASH_FUNCTION_OUTPUT_LENGTH * sizeof(md5_byte_t))
  {
      *partialhash = (md5_byte_t*) malloc(HASH_FUNCTION_OUTPUT_LENGTH * sizeof(md5_byte_t));
      if (*partialhash == NULL) {
          errormsg("out of memory\n");
          exit(1);
      }

      md5copy(*partialhash, sqlite3_column_blob(query_loadhash, 0));
  }
  else
  {
      *partialhash = 0;
  }

  hashsize = sqlite3_column_bytes(query_loadhash, 1);

  if (hashsize == HASH_FUNCTION_OUTPUT_LENGTH * sizeof(md5_byte_t))
  {
      *fullhash = (md5_byte_t*) malloc(HASH_FUNCTION_OUTPUT_LENGTH * sizeof(md5_byte_t));
      if (*fullhash == NULL) {
          errormsg("out of memory\n");
          exit(1);
      }

      md5copy(*fullhash, sqlite3_column_blob(query_loadhash, 1));
  }
  else
  {
      *fullhash = 0;
  }

  sqlite3_reset(query_loadhash);

  return *partialhash || *fullhash;
}

int hashdb_savehash(sqlite3 *db, const file_t *entry, md5_byte_t *partialhash, md5_byte_t *fullhash)
{
  int result;
  char *realpath;
  char *name;
  sqlite3_int64 directoryid;

  realpath = getrealpath(entry->d_name, 0);
  if (realpath == 0)
    return 0;

  name = malloc(strlen(realpath) + 1);
  if (name == 0)
  {
    free(realpath);
    return 0;
  }

  sdirname(name, realpath);

  if (!hashdb_getdirectoryid(db, name, &directoryid))
  {
    if (!hashdb_savedirectory(db, name))
    {
      free(name);
      free(realpath);
      return 0;
    }

    directoryid = sqlite3_last_insert_rowid(db);
  }

  sbasename(name, realpath);

  sqlite3_bind_int64(query_savehash, 1, directoryid);
  sqlite3_bind_text(query_savehash, 2, name, strlen(name), SQLITE_TRANSIENT);
  sqlite3_bind_blob(query_savehash, 3, &entry->inode, sizeof(entry->inode), SQLITE_TRANSIENT);
  sqlite3_bind_int64(query_savehash, 4, entry->size);
  sqlite3_bind_blob(query_savehash, 5, &entry->ctime, sizeof(entry->ctime), SQLITE_TRANSIENT);
  sqlite3_bind_blob(query_savehash, 6, &entry->mtime, sizeof(entry->mtime), SQLITE_TRANSIENT);
  sqlite3_bind_int64(query_savehash, 7, entry->ctime_nsec);
  sqlite3_bind_int64(query_savehash, 8, entry->mtime_nsec);

  if (partialhash)
    sqlite3_bind_blob(query_savehash, 9, partialhash, HASH_FUNCTION_OUTPUT_LENGTH * sizeof(md5_byte_t), SQLITE_TRANSIENT);
  else
    sqlite3_bind_null(query_savehash, 9);

  sqlite3_bind_int64(query_savehash, 10, PARTIAL_MD5_SIZE);

  if (fullhash)
    sqlite3_bind_blob(query_savehash, 11, fullhash, HASH_FUNCTION_OUTPUT_LENGTH * sizeof(md5_byte_t), SQLITE_TRANSIENT);
  else
    sqlite3_bind_null(query_savehash, 11);

  sqlite3_bind_int(query_savehash, 12, HASH_FUNCTION);

  result = sqlite3_step(query_savehash);

  free(name);
  free(realpath);

  sqlite3_reset(query_savehash);

  return result == SQLITE_DONE;
}

int hashdb_foreachhash(sqlite3 *db, sqlite3_int64 *directoryid, int (*callback)(const sqlite3_int64, const char*, const char*))
{
  int result;
  sqlite3_stmt *query;

  if (directoryid != 0) {
    query = query_foreachhashwithin;
    sqlite3_bind_int64(query, 1, *directoryid);
  } else {
    query = query_foreachhash;
    sqlite3_bind_null(query, 1);
  }

  result = sqlite3_step(query);
  while (result == SQLITE_ROW)
  {
    result = callback(
      sqlite3_column_int64(query, 0),
      sqlite3_column_text(query, 1),
      sqlite3_column_text(query, 2)
    );

    if (result == 0) {
      sqlite3_reset(query);
      return 1;
    }

    result = sqlite3_step(query);
  }

  sqlite3_reset(query);

  return result == SQLITE_DONE;
}

int hashdb_deletehash(sqlite3 *db, sqlite3_int64 directoryid, const char *filename)
{
  int result;

  sqlite3_bind_int64(query_deletehash, 1, directoryid);
  sqlite3_bind_text(query_deletehash, 2, filename, strlen(filename), SQLITE_TRANSIENT);

  result = sqlite3_step(query_deletehash);

  sqlite3_reset(query_deletehash);

  return result == SQLITE_DONE;
}

int hashdb_deletehashforpath(sqlite3 *db, const char *path)
{
  int result;
  char *name;
  sqlite3_int64 pathid;

  name = malloc(strlen(path) + 1);
  if (name == 0)
    return 0;

  sbasename(name, path);
  sqlite3_bind_text(query_deletehashforpath, 1, name, strlen(name), SQLITE_TRANSIENT);

  sdirname(name, path);
  sqlite3_bind_text(query_deletehashforpath, 2, name, strlen(name), SQLITE_TRANSIENT);

  free(name);

  result = sqlite3_step(query_deletehashforpath);

  sqlite3_reset(query_deletehashforpath);

  return result == SQLITE_DONE;
}