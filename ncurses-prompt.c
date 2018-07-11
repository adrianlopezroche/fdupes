#include <stdlib.h>
#include "ncurses-print.h"
#include "ncurses-prompt.h"

struct prompt_info *prompt_info_alloc(size_t initial_size)
{
  struct prompt_info *out;

  if (initial_size < 1)
    return 0;

  out = (struct prompt_info*) malloc(sizeof(struct prompt_info));
  if (out == 0)
    return 0;

  out->text = malloc(initial_size * sizeof(wchar_t));
  if (out->text == 0)
  {
    free(out);
    return 0;
  }

  out->text[0] = L'\0';
  out->allocated = initial_size;
  out->offset = 0;
  out->cursor = 0;
  out->active = 0;

  return out;
}

void free_prompt_info(struct prompt_info *info)
{
  free(info->text);
  free(info);
}

int format_prompt(struct prompt_info *prompt, wchar_t *format, ...)
{
  va_list ap;
  va_list aq;
  int size;
  wchar_t *newtext;

  va_start(ap, format);
  va_copy(aq, ap);

  size = vwprintflength(format, aq);

  if (size + 3 > prompt->allocated)
  {
    newtext = (wchar_t*)realloc(prompt->text, (size + 3) * sizeof(wchar_t));

    if (newtext == 0)
      return 0;

    prompt->text = newtext;
    prompt->allocated = size + 1;
  }

  vswprintf(prompt->text, prompt->allocated, format, ap);

  size = wcslen(prompt->text);

  prompt->text[size + 0] = L':';
  prompt->text[size + 1] = L' ';
  prompt->text[size + 2] = L'\0';

  va_end(aq);
  va_end(ap);

  return 1;
}

void set_prompt_active_state(struct prompt_info *prompt, int active)
{
  prompt->active = active;
}

void update_prompt(WINDOW *promptwin, struct prompt_info *prompt, wchar_t *commandbuffer, int cursor_delta)
{
  const size_t cursor_stop = wcslen(prompt->text);
  const size_t right_edge = getmaxx(promptwin);
  const size_t cursor_position = cursor_stop + prompt->cursor - prompt->offset;

  if (cursor_delta > 0)
  {
    if (prompt->cursor + cursor_delta > wcslen(commandbuffer))
     cursor_delta = wcslen(commandbuffer) - prompt->cursor;

    if (cursor_position + cursor_delta >= right_edge)
      prompt->offset += cursor_delta;
  }
  else if (cursor_delta < 0)
  {
    if (-cursor_delta > prompt->cursor)
      cursor_delta = -(int)prompt->cursor;

    if (cursor_position + cursor_delta < cursor_stop)
      prompt->offset += cursor_delta;
  }

  prompt->cursor += cursor_delta;
}

void print_prompt(WINDOW *promptwin, struct prompt_info *prompt, wchar_t *commandbuffer)
{
  if (prompt->active)
    prompt->text[wcslen(prompt->text) - 2] = '=';
  else
    prompt->text[wcslen(prompt->text) - 2] = ':';

  werase(promptwin);

  if (prompt->offset <= wcslen(prompt->text))
  {
    wattron(promptwin, A_BOLD);
    waddwstr(promptwin, prompt->text + prompt->offset);
    wattroff(promptwin, A_BOLD);

    waddwstr(promptwin, commandbuffer);
  }
  else if (prompt->offset < wcslen(prompt->text) + wcslen(commandbuffer))
  {
    waddwstr(promptwin, commandbuffer + prompt->offset - wcslen(prompt->text));
  }
}