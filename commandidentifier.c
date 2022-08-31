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

#include <stdlib.h>
#include "commandidentifier.h"

/* insert new node into command identifier tree */
int insert_command_identifier_command(struct command_identifier_node *tree, struct command_map *command, size_t ch)
{
  struct command_identifier_node *child;
  struct command_identifier_node **alloc_children;
  int returned_command;
  int c;

  /* find node for current character in command name */
  child = 0;
  for (c = 0; c < tree->num_children; ++c)
  {
    if (tree->children[c]->character == command->command_name[ch])
    {
      child = tree->children[c];
      break;
    }
  }

  /* if sought node does not exist, create it */
  if (child == 0)
  {
    child = (struct command_identifier_node*) malloc(sizeof(struct command_identifier_node));
    if (child == 0)
      return COMMAND_RECOGNIZER_OUT_OF_MEMORY;

    child->command = COMMAND_UNDEFINED;
    child->character = command->command_name[ch];
    child->children = 0;
    child->num_children = 0;

    alloc_children = realloc(tree->children, sizeof(struct command_identifier_node*) * (tree->num_children + 1));
    if (alloc_children == 0)
      return COMMAND_RECOGNIZER_OUT_OF_MEMORY;

    tree->children = alloc_children;

    tree->children[tree->num_children++] = child;
  }

  /* if last character in command name, make child a leaf node */
  if (command->command_name[ch] == L'\0')
  {
    child->command = command->command;
    return child->command;
  }
  else /* grow the tree */
  {
    returned_command = insert_command_identifier_command(child, command, ch + 1);

    /* record whether the tree at this point leads to a single command (abbreviation) or many (ambiguous) */
    if (tree->command == COMMAND_UNDEFINED)
      tree->command = returned_command;
    else
      tree->command = COMMAND_AMBIGUOUS;
  }

  return tree->command;
}

/* compare two command identifier nodes by the characters they match */
int compare_command_identifier_nodes(const void *a, const void *b)
{
  const struct command_identifier_node *aa;
  const struct command_identifier_node *bb;

  aa = *(struct command_identifier_node**)a;
  bb = *(struct command_identifier_node**)b;

  if (aa->character > bb->character)
    return 1;
  else if (aa->character < bb->character)
    return -1;
  else
    return 0;
}

/* sort command identifier nodes in alphabetical order */
void sort_command_identifier_nodes(struct command_identifier_node *root)
{
  int c;

  if (root->num_children > 1)
    qsort(root->children, root->num_children, sizeof(struct command_identifier_node *), compare_command_identifier_nodes);

  for (c = 0; c < root->num_children; ++c)
    sort_command_identifier_nodes(root->children[c]);
}

/* build tree to identify command names through state transitions */
struct command_identifier_node *build_command_identifier_tree(struct command_map *commands)
{
  struct command_identifier_node *root;
  int c;

  root = (struct command_identifier_node*) malloc(sizeof(struct command_identifier_node));
  if (root == 0)
    return 0;

  root->command = COMMAND_UNDEFINED;
  root->character = L'\0';
  root->children = 0;
  root->num_children = 0;

  c = 0;
  while (commands[c].command_name != 0)
  {
    insert_command_identifier_command(root, commands + c, 0);
    ++c;
  }

  sort_command_identifier_nodes(root);

  return root;
}

/* free memory used by command identifier tree structure */
void free_command_identifier_tree(struct command_identifier_node *tree)
{
  int c;

  for (c = 0; c < tree->num_children; ++c)
    free_command_identifier_tree(tree->children[c]);

  free(tree->children);
  free(tree);
}

/* find command identifier node matching given character */
struct command_identifier_node *find_command_identifier_node(struct command_identifier_node *root, wchar_t character)
{
  long min;
  long max;
  long mid;

  if (root->num_children == 0)
    return 0;

  min = 0;
  max = root->num_children - 1;

  do
  {
    mid = (min + max) / 2;

    if (character > root->children[mid]->character)
      min = mid + 1;
    else if (character < root->children[mid]->character)
      max = mid - 1;
    else
      return root->children[mid];
  } while (min <= max);

  return 0;
}

/* determine ID for given command string (possibly abbreviated), if found */
int identify_command(struct command_identifier_node *tree, wchar_t *command_buffer, size_t ch)
{
  struct command_identifier_node *found;

  if (command_buffer[ch] != L' ')
    found = find_command_identifier_node(tree, command_buffer[ch]);
  else
    found = find_command_identifier_node(tree, L'\0');

  if (found)
  {
    if (command_buffer[ch] == L'\0' || command_buffer[ch] == L' ')
      return found->command;
    else
      return identify_command(found, command_buffer, ch + 1);
  }
  else
  {
    if (command_buffer[ch] == L'\0' || command_buffer[ch] == L' ')
      return tree->command;
    else
      return COMMAND_UNDEFINED;
  }
}
