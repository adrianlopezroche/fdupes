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