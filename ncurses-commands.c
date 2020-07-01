/* FDUPES Copyright (c) 2018 Adrian Lopez

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
#include "ncurses-status.h"
#include "ncurses-commands.h"
#include "wcs.h"
#include "mbstowcs_escape_invalid.h"
#include "log.h"
#include <wchar.h>
#include <pcre2.h>

void set_file_action(struct groupfile *file, int new_action, size_t *deletion_tally);

struct command_map command_list[] = {
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
  {L"csel", COMMAND_CLEAR_ALL_SELECTIONS},
  {L"isel", COMMAND_INVERT_GROUP_SELECTIONS},
  {L"ks", COMMAND_KEEP_SELECTED},
  {L"ds", COMMAND_DELETE_SELECTED},
  {L"rs", COMMAND_RESET_SELECTED},
  {L"rg", COMMAND_RESET_GROUP},
  {L"all", COMMAND_PRESERVE_ALL},
  {L"goto", COMMAND_GOTO_SET},
  {L"prune", COMMAND_PRUNE},
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
  wchar_t *wcsfilename;
  size_t needed;
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

  code = pcre2_compile((PCRE2_SPTR)commandarguments, PCRE2_ZERO_TERMINATED, PCRE2_UTF | PCRE2_UCP, &errorcode, &erroroffset, 0);

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
      needed = mbstowcs_escape_invalid(0, groups[g].files[f].file->d_name, 0);

      wcsfilename = (wchar_t*) malloc(needed * sizeof(wchar_t));
      if (wcsfilename == 0)
        continue;

      mbstowcs_escape_invalid(wcsfilename, groups[g].files[f].file->d_name, needed);

      matches = pcre2_match(code, (PCRE2_SPTR)wcsfilename, PCRE2_ZERO_TERMINATED, 0, 0, md, 0);

      free(wcsfilename);

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
  wchar_t *wcsfilename;
  size_t needed;
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

  code = pcre2_compile((PCRE2_SPTR)commandarguments, PCRE2_ZERO_TERMINATED, PCRE2_UTF | PCRE2_UCP, &errorcode, &erroroffset, 0);

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
      needed = mbstowcs_escape_invalid(0, groups[g].files[f].file->d_name, 0);

      wcsfilename = (wchar_t*) malloc(needed * sizeof(wchar_t));
      if (wcsfilename == 0)
        continue;

      mbstowcs_escape_invalid(wcsfilename, groups[g].files[f].file->d_name, needed);

      matches = pcre2_match(code, (PCRE2_SPTR)wcsfilename, PCRE2_ZERO_TERMINATED, 0, 0, md, 0);

      free(wcsfilename);

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

  if (status)
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

int filerowcount(file_t *file, const int columns, int group_file_count);

/* delete files tagged for deletion, delist sets with no untagged files */
int cmd_prune(struct filegroup *groups, int groupcount, wchar_t *commandarguments, size_t *deletiontally, int *totalgroups, int *cursorgroup, int *cursorfile, int *topline, char *logfile, WINDOW *filewin, struct status_text *status)
{
  int deletecount;
  int preservecount;
  int unresolvedcount;
  int totaldeleted = 0;
  double deletedbytes = 0;
  struct log_info *loginfo;
  int g;
  int f;
  int to;
  int adjusttopline;
  int toplineoffset;
  int groupfirstline;

  if (logfile != 0)
    loginfo = log_open(logfile, 0);
  else
    loginfo = 0;

  for (g = 0; g < *totalgroups; ++g)
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

    if (loginfo)
      log_begin_set(loginfo);

    /* delete files marked for deletion unless no files left undeleted */
    if (deletecount < groups[g].filecount)
    {
      for (f = 0; f < groups[g].filecount; ++f)
      {
        if (groups[g].files[f].action == -1)
        {
          if (remove(groups[g].files[f].file->d_name) == 0)
          {
            set_file_action(&groups[g].files[f], -2, deletiontally);

            deletedbytes += groups[g].files[f].file->size;
            ++totaldeleted;

            if (loginfo)
              log_file_deleted(loginfo, groups[g].files[f].file->d_name);
          }
        }
      }

      if (loginfo)
      {
        for (f = 0; f < groups[g].filecount; ++f)
        {
          if (groups[g].files[f].action >= 0)
            log_file_remaining(loginfo, groups[g].files[f].file->d_name);
        }
      }

      deletecount = 0;
    }

    if (loginfo)
      log_end_set(loginfo);

    /* if no files left unresolved, mark preserved files for delisting */
    if (unresolvedcount == 0)
    {
      for (f = 0; f < groups[g].filecount; ++f)
        if (groups[g].files[f].action == 1)
          set_file_action(&groups[g].files[f], -2, deletiontally);

      preservecount = 0;
    }
    /* if only one file left unresolved, mark it for delesting */
    else if (unresolvedcount == 1 && preservecount + deletecount == 0)
    {
      for (f = 0; f < groups[g].filecount; ++f)
        if (groups[g].files[f].action == 0)
          set_file_action(&groups[g].files[f], -2, deletiontally);
    }

    /* delist any files marked for delisting */
    to = 0;
    for (f = 0; f < groups[g].filecount; ++f)
      if (groups[g].files[f].action != -2)
        groups[g].files[to++] = groups[g].files[f];

    groups[g].filecount = to;

    /* reposition cursor, if necessary */
    if (*cursorgroup == g && *cursorfile > 0 && *cursorfile >= groups[g].filecount)
      *cursorfile = groups[g].filecount - 1;
  }

  if (loginfo != 0)
    log_close(loginfo);

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
  for (g = 0; g < *totalgroups; ++g)
  {
    if (groups[g].filecount > 0)
    {
      groups[to] = groups[g];

      /* reposition cursor, if necessary */
      if (to == *cursorgroup && to != g)
        *cursorfile = 0;

      ++to;
    }
    else
    {
      free(groups[g].files);
    }
  }

  *totalgroups = to;

  /* reposition cursor, if necessary */
  if (*cursorgroup >= *totalgroups)
  {
    *cursorgroup = *totalgroups - 1;
    *cursorfile = 0;
  }

  /* recalculate line boundaries */
  adjusttopline = 1;
  toplineoffset = 0;
  groupfirstline = 0;

  for (g = 0; g < *totalgroups; ++g)
  {
    if (adjusttopline && groups[g].endline >= *topline)
      toplineoffset = groups[g].endline - *topline;

    groups[g].startline = groupfirstline;
    groups[g].endline = groupfirstline + 2;

    for (f = 0; f < groups[g].filecount; ++f)
      groups[g].endline += filerowcount(groups[g].files[f].file, COLS, groups[g].filecount);

    if (adjusttopline && toplineoffset > 0)
    {
      *topline = groups[g].endline - toplineoffset;

      if (*topline < 0)
        *topline = 0;

      adjusttopline = 0;
    }

    groupfirstline = groups[g].endline + 1;
  }

  if (*totalgroups > 0 && groups[*totalgroups-1].endline <= *topline)
  {
    *topline = groups[*totalgroups-1].endline - getmaxy(filewin) + 1;

    if (*topline < 0)
      *topline = 0;
  }

  cmd_clear_all_selections(groups, *totalgroups, commandarguments, 0);
}