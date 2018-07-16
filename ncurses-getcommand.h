#ifndef NCURSESGETCOMMAND_H
#define NCURSESGETCOMMAND_H

#include <wchar.h>
#ifdef HAVE_NCURSESW_CURSES_H
  #include <ncursesw/curses.h>
#else
  #include <curses.h>
#endif
#include "ncurses-prompt.h"

#define GET_COMMAND_OK 1
#define GET_COMMAND_CANCELED 2
#define GET_COMMAND_ERROR_OUT_OF_MEMORY 3
#define GET_COMMAND_RESIZE_REQUESTED 4
#define GET_COMMAND_KEY_NPAGE 5
#define GET_COMMAND_KEY_PPAGE 6
#define GET_COMMAND_KEY_SF 7
#define GET_COMMAND_KEY_SR 8

void get_command_arguments(wchar_t **arguments, wchar_t *input);
int get_command_text(wchar_t **commandbuffer, size_t *commandbuffersize, WINDOW *promptwin, struct prompt_info *prompt, int cancel_on_erase, int append);

#endif