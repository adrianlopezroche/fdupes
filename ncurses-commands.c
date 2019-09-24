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
