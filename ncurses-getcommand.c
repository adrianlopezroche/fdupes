#include <stdlib.h>
#include <signal.h>
#include "ncurses-getcommand.h"

#define KEY_ESCAPE 27

extern volatile sig_atomic_t got_sigint;

/* get command and arguments from user input */
void get_command_arguments(wchar_t **arguments, wchar_t *input)
{
  size_t l;
  size_t x;

  l = wcslen(input);

  for (x = 0; x < l; ++x)
    if (input[x] == L' ')
      break;

  if (input[x] == L' ')
    *arguments = input + x + 1;
  else
    *arguments = input + x;
}

int get_command_text(wchar_t **commandbuffer, size_t *commandbuffersize, WINDOW *promptwin, struct prompt_info *prompt, int cancel_on_erase, int append)
{
  int docommandinput;
  int keyresult;
  wint_t wch;
  wint_t oldch;
  size_t length;
  size_t newsize;
  wchar_t *realloccommandbuffer;
  size_t c;

  if (*commandbuffer == 0)
  {
    *commandbuffersize = 80;
    *commandbuffer = malloc(*commandbuffersize * sizeof(wchar_t));
    if (*commandbuffer == 0)
      return GET_COMMAND_ERROR_OUT_OF_MEMORY;
  }

  if (!append)
  {
    (*commandbuffer)[0] = L'\0';
  }
  else
  {
    print_prompt(promptwin, prompt, *commandbuffer);

    prompt->cursor = wcswidth(*commandbuffer, wcslen(*commandbuffer));

    wmove(promptwin, 0, wcslen(prompt->text) + prompt->cursor - prompt->offset);

    wrefresh(promptwin);
  }

  docommandinput = 1;
  do
  {
    do
    {
      keyresult = wget_wch(promptwin, &wch);

      if (got_sigint)
      {
        got_sigint = 0;

        (*commandbuffer)[0] = '\0';

        return GET_COMMAND_CANCELED;
      }
    } while (keyresult == ERR);

    if (keyresult == OK)
    {
      switch (wch)
      {
        case KEY_ESCAPE:
          prompt->offset = 0;
          prompt->cursor = 0;
          docommandinput = 0;

          (*commandbuffer)[0] = '\0';

          return GET_COMMAND_CANCELED;

        case '\n':
          prompt->offset = 0;
          prompt->cursor = 0;
          docommandinput = 0;
          continue;

        case '\t':
          continue;

        default:
          if (!iswprint(wch))
            continue;

          length = wcslen(*commandbuffer);

          if (length + 1 >= *commandbuffersize)
          {
            newsize = *commandbuffersize * 2;

            realloccommandbuffer = (wchar_t*)realloc(*commandbuffer, newsize * sizeof(wchar_t));
            if (realloccommandbuffer == 0)
              return GET_COMMAND_ERROR_OUT_OF_MEMORY;

            *commandbuffer = realloccommandbuffer;
            *commandbuffersize = newsize;
          }

          for (c = length + 1; c >= prompt->cursor + 1; --c)
            (*commandbuffer)[c] = (*commandbuffer)[c-1];

          (*commandbuffer)[prompt->cursor] = wch;

          update_prompt(promptwin, prompt, *commandbuffer, wcwidth(wch));

          break;
      }
    }
    else if (keyresult == KEY_CODE_YES)
    {
      switch (wch)
      {
        case KEY_BACKSPACE:
          length = wcslen(*commandbuffer);

          oldch = (*commandbuffer)[prompt->cursor];

          if (prompt->cursor > 0)
            for (c = prompt->cursor; c <= length; ++c)
              (*commandbuffer)[c-1] = (*commandbuffer)[c];

          update_prompt(promptwin, prompt, *commandbuffer, oldch != 0 ? -wcwidth(oldch) : -1);

          if (cancel_on_erase && wcslen(*commandbuffer) == 0)
            return GET_COMMAND_CANCELED;

          break;

        case KEY_DC:
          length = wcslen(*commandbuffer);

          if (prompt->cursor < length)
            for (c = prompt->cursor; c <= length; ++c)
              (*commandbuffer)[c] = (*commandbuffer)[c+1];

          if (cancel_on_erase && wcslen(*commandbuffer) == 0)
            return GET_COMMAND_CANCELED;

          break;

        case KEY_LEFT:
          length = wcslen(*commandbuffer);

          oldch = (*commandbuffer)[prompt->cursor];

          update_prompt(promptwin, prompt, *commandbuffer, oldch != 0 ? -wcwidth(oldch) : -1);

          break;

        case KEY_RIGHT:
          length = wcslen(*commandbuffer);

          oldch = (*commandbuffer)[prompt->cursor];

          if (prompt->cursor + wcwidth((*commandbuffer)[prompt->cursor]) <= length)
            update_prompt(promptwin, prompt, *commandbuffer, wcwidth(oldch));

          break;

        case KEY_NPAGE:
          return GET_COMMAND_KEY_NPAGE;

        case KEY_PPAGE:
          return GET_COMMAND_KEY_PPAGE;

        case KEY_SF:
          return GET_COMMAND_KEY_SF;

        case KEY_SR:
          return GET_COMMAND_KEY_SR;

        case KEY_RESIZE:
          return GET_COMMAND_RESIZE_REQUESTED;
      }
    }

    print_prompt(promptwin, prompt, *commandbuffer);

    wmove(promptwin, 0, wcslen(prompt->text) + prompt->cursor - prompt->offset);

    wrefresh(promptwin);
  } while (docommandinput);

  return GET_COMMAND_OK;
}