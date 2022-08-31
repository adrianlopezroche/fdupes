/* Copyright (c) 2018-2022 Adrian Lopez

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
      claim that you wrote the original software. If you use this software
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.
   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original software.
   3. This notice may not be removed or altered from any source distribution. */

#ifndef COMMANDIDENTIFIER_H
#define COMMANDIDENTIFIER_H

#include <wchar.h>

#define COMMAND_RECOGNIZER_OUT_OF_MEMORY -2
#define COMMAND_AMBIGUOUS -1
#define COMMAND_UNDEFINED 0

/* command name to command ID map structure */
struct command_map {
  wchar_t *command_name;
  int command;
};

/* command identifier node structure */
struct command_identifier_node
{
  int command;
  wchar_t character;
  struct command_identifier_node **children;
  size_t num_children;
};

int identify_command(struct command_identifier_node *tree, wchar_t *command_buffer, size_t ch);
struct command_identifier_node *build_command_identifier_tree(struct command_map *commands);
void free_command_identifier_tree(struct command_identifier_node *tree);

#endif