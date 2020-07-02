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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#ifdef HAVE_NCURSESW_CURSES_H
  #include <ncursesw/curses.h>
#else
  #include <curses.h>
#endif
#include "ncurses-interface.h"
#include "ncurses-getcommand.h"
#include "ncurses-commands.h"
#include "ncurses-prompt.h"
#include "ncurses-status.h"
#include "ncurses-print.h"
#include "mbstowcs_escape_invalid.h"
#include "positive_wcwidth.h"
#include "commandidentifier.h"
#include "filegroup.h"
#include "errormsg.h"
#include "log.h"
#include "sigint.h"
#include "flags.h"

char *fmttime(time_t t);

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
  size_t x = 0;
  size_t read;
  size_t filename_bytes;
  wchar_t ch;
  mbstate_t mbstate;
  int index_width;
  int timestamp_width;
  size_t needed;
  wchar_t *wcfilename;

  memset(&mbstate, 0, sizeof(mbstate));

  needed = mbstowcs_escape_invalid(0, file->d_name, 0);

  wcfilename = (wchar_t*)malloc(sizeof(wchar_t) * needed);
  if (wcfilename == 0)
    return 0;

  mbstowcs_escape_invalid(wcfilename, file->d_name, needed);

  index_width = get_num_digits(group_file_count);
  if (index_width < FILE_INDEX_MIN_WIDTH)
    index_width = FILE_INDEX_MIN_WIDTH;

  timestamp_width = ISFLAG(flags, F_SHOWTIME) ? 19 : 0;

  lines = (index_width + timestamp_width + FILENAME_INDENT_EXTRA) / columns + 1;

  line_remaining = columns - (index_width + timestamp_width + FILENAME_INDENT_EXTRA) % columns;

  while (wcfilename[x] != L'\0')
  {
    if (positive_wcwidth(wcfilename[x]) <= line_remaining)
    {
      line_remaining -= positive_wcwidth(wcfilename[x]);
    }
    else
    {
      line_remaining = columns - positive_wcwidth(wcfilename[x]);
      ++lines;
    }

    ++x;
  }

  free(wcfilename);

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

void deletefiles_ncurses(file_t *files, char *logfile)
{
  WINDOW *filewin;
  WINDOW *promptwin;
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
  size_t globaldeletiontally = 0;
  int row;
  int x;
  int g;
  wint_t wch;
  int keyresult;
  int cy;
  int f;
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
  struct prompt_info *prompt;
  int dupesfound;
  int intresult;
  int resumecommandinput = 0;
  int index_width;
  int timestamp_width;

  noecho();
  cbreak();
  halfdelay(5);

  filewin = newwin(LINES - 2, COLS, 0, 0);
  statuswin = newwin(1, COLS, LINES - 1, 0);
  promptwin = newwin(1, COLS, LINES - 2, 0);

  scrollok(filewin, FALSE);
  scrollok(statuswin, FALSE);
  scrollok(promptwin, FALSE);

  wattron(statuswin, A_REVERSE);

  keypad(promptwin, 1);

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

  register_sigint_handler();

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
      free(commandbuffer);
      free_command_identifier_tree(commandidentifier);
      free_command_identifier_tree(confirmationkeywordidentifier);

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
  if (status == 0)
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

  format_status_left(status, L"Ready");

  prompt = prompt_info_alloc(80);
  if (prompt == 0)
  {
    free_status_text(status);

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

  doprune = 1;
  do
  {
    wmove(filewin, 0, 0);
    werase(filewin);

    if (totalgroups > 0)
      totallines = groups[totalgroups-1].endline;
    else
      totallines = 0;

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

      timestamp_width = ISFLAG(flags, F_SHOWTIME) ? 19 : 0;

      linestyle = getlinestyle(groups + groupindex, x);
      
      if (linestyle == linestyle_groupheader)
      {
        wattron(filewin, A_BOLD);
        if (groups[groupindex].selected)
          wattron(filewin, A_REVERSE);
        wprintw(filewin, "Set %d of %d:\n", groupindex + 1, totalgroups);
        if (groups[groupindex].selected)
          wattroff(filewin, A_REVERSE);
        wattroff(filewin, A_BOLD);
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

            if (ISFLAG(flags, F_SHOWTIME))
              wprintw(filewin, "[%s] ", fmttime(groups[groupindex].files[f].file->mtime));
          }

          cy = getcury(filewin);

          if (groups[groupindex].files[f].selected)
            wattron(filewin, A_REVERSE);
          putline(filewin, groups[groupindex].files[f].file->d_name, row, COLS, index_width + timestamp_width + FILENAME_INDENT_EXTRA);
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

            if (ISFLAG(flags, F_SHOWTIME))
              wprintw(filewin, "[%s] ", fmttime(groups[groupindex].files[f].file->mtime));
          }

          cy = getcury(filewin);

          if (groups[groupindex].files[f].selected)
            wattron(filewin, A_REVERSE);
          putline(filewin, groups[groupindex].files[f].file->d_name, row, COLS, index_width + timestamp_width + FILENAME_INDENT_EXTRA);
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

    if (totalgroups > 0)
      format_status_right(status, L"Set %d of %d", cursorgroup+1, totalgroups);
    else
      format_status_right(status, L"Finished");

    print_status(statuswin, status);

    if (totalgroups > 0)
      format_prompt(prompt, L"( Preserve files [1 - %d, all, help] )", groups[cursorgroup].filecount);
    else if (dupesfound)
      format_prompt(prompt, L"( No duplicates remaining; type 'exit' to exit program )");
    else
      format_prompt(prompt, L"( No duplicates found; type 'exit' to exit program )");

    print_prompt(promptwin, prompt, L"");

    /* refresh windows (using wrefresh instead of wnoutrefresh to avoid bug in gnome-terminal) */
    wrefresh(filewin);
    wrefresh(statuswin);
    wrefresh(promptwin);

    /* wait for user input */
    if (!resumecommandinput)
    {
      do
      {
        keyresult = wget_wch(promptwin, &wch);

        if (got_sigint)
        {
          getyx(promptwin, cursor_y, cursor_x);

          format_status_left(status, L"Type 'exit' to exit fdupes.");
          print_status(statuswin, status);

          wmove(promptwin, cursor_y, cursor_x);

          got_sigint = 0;

          wrefresh(statuswin);
        }
      } while (keyresult == ERR);

      if (keyresult == OK && iswprint(wch))
      {
        commandbuffer[0] = wch;
        commandbuffer[1] = '\0';
      }
      else
      {
        commandbuffer[0] = '\0';
      }
    }

    if (resumecommandinput || (keyresult == OK && iswprint(wch) && ((wch != '\t' && wch != '\n' && wch != '?'))))
    {
      resumecommandinput = 0;

      switch (get_command_text(&commandbuffer, &commandbuffersize, promptwin, prompt, 1, 1))
      {
        case GET_COMMAND_OK:
          format_status_left(status, L"Ready");

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

            case COMMAND_PRUNE:
              cmd_prune(groups, totalgroups, commandarguments, &globaldeletiontally, &totalgroups, &cursorgroup, &cursorfile, &topline, logfile, filewin, status);
              break;

            case COMMAND_EXIT: /* exit program */
              if (totalgroups == 0)
              {
                doprune = 0;
                continue;
              }
              else
              {
                if (globaldeletiontally != 0)
                  format_prompt(prompt, L"( There are files marked for deletion. Exit without deleting? )");
                else
                  format_prompt(prompt, L"( There are duplicates remaining. Exit anyway? )");

                print_prompt(promptwin, prompt, L"");

                wrefresh(promptwin);

                switch (get_command_text(&commandbuffer, &commandbuffersize, promptwin, prompt, 0, 0))
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

                    wresize(statuswin, 1, COLS);
                    wresize(promptwin, 1, COLS);
                    mvwin(statuswin, LINES - 1, 0);
                    mvwin(promptwin, LINES - 2, 0);

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
          break;

        case GET_COMMAND_RESIZE_REQUESTED:
          /* resize windows */
          wresize(filewin, LINES - 2, COLS);

          wresize(statuswin, 1, COLS);
          wresize(promptwin, 1, COLS);
          mvwin(statuswin, LINES - 1, 0);
          mvwin(promptwin, LINES - 2, 0);

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

      case KEY_SRIGHT:
        set_file_action(&groups[cursorgroup].files[cursorfile], 1, &globaldeletiontally);

        format_status_left(status, L"1 file marked for preservation.");

        if (cursorfile < groups[cursorgroup].filecount - 1)
          move_to_next_file(&topline, &cursorgroup, &cursorfile, groups, filewin);
        else if (cursorgroup < totalgroups - 1)
          move_to_next_group(&topline, &cursorgroup, &cursorfile, groups, filewin);

        break;

      case KEY_SLEFT:
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
          --cursorgroup;

        cursorfile = 0;

        scroll_to_group(&topline, cursorgroup, 0, groups, filewin);

        break;

      case KEY_F(3):
        move_to_next_selected_group(&topline, &cursorgroup, &cursorfile, groups, totalgroups, filewin);
        break;

      case KEY_F(2):
        move_to_previous_selected_group(&topline, &cursorgroup, &cursorfile, groups, totalgroups, filewin);
        break;

      case KEY_DC:
        cmd_prune(groups, totalgroups, commandarguments, &globaldeletiontally, &totalgroups, &cursorgroup, &cursorfile, &topline, logfile, filewin, status);
        break;

      case KEY_RESIZE:
        /* resize windows */
        wresize(filewin, LINES - 2, COLS);

        wresize(statuswin, 1, COLS);
        wresize(promptwin, 1, COLS);
        mvwin(statuswin, LINES - 1, 0);
        mvwin(promptwin, LINES - 2, 0);

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

  free_prompt_info(prompt);

  free_status_text(status);

  free_command_identifier_tree(commandidentifier);
  free_command_identifier_tree(confirmationkeywordidentifier);

  for (g = 0; g < totalgroups; ++g)
    free(groups[g].files);

  free(groups);
}
