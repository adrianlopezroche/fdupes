#pragma once

#include "commandidentifier.h"
#include "filegroup.h"

/* command IDs */
#define COMMAND_SELECT_CONTAINING 1
#define COMMAND_SELECT_BEGINNING 2
#define COMMAND_SELECT_ENDING 3
#define COMMAND_SELECT_MATCHING 4
#define COMMAND_CLEAR_SELECTIONS_CONTAINING 5
#define COMMAND_CLEAR_SELECTIONS_BEGINNING 6
#define COMMAND_CLEAR_SELECTIONS_ENDING 7
#define COMMAND_CLEAR_SELECTIONS_MATCHING 8
#define COMMAND_CLEAR_ALL_SELECTIONS 9
#define COMMAND_INVERT_GROUP_SELECTIONS 10
#define COMMAND_KEEP_SELECTED 11
#define COMMAND_DELETE_SELECTED 12
#define COMMAND_RESET_SELECTED 13
#define COMMAND_RESET_GROUP 14
#define COMMAND_PRESERVE_ALL 15
#define COMMAND_EXIT 16
#define COMMAND_HELP 17
#define COMMAND_YES 18
#define COMMAND_NO 19
#define COMMAND_SELECT_REGEX 20
#define COMMAND_CLEAR_SELECTIONS_REGEX 21
#define COMMAND_GOTO_SET 22

extern struct command_map command_list[];
extern struct command_map confirmation_keyword_list[];

struct filegroup;
struct status_text;

int cmd_select_containing(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status);
int cmd_select_beginning(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status);
int cmd_select_ending(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status);
int cmd_select_matching(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status);
int cmd_select_regex(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status);
int cmd_clear_selections_containing(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status);
int cmd_clear_selections_beginning(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status);
int cmd_clear_selections_ending(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status);
int cmd_clear_selections_matching(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status);
int cmd_clear_selections_regex(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status);
int cmd_clear_all_selections(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status);
int cmd_invert_group_selections(struct filegroup *groups, int groupcount, wchar_t *commandarguments, struct status_text *status);
int cmd_keep_selected(struct filegroup *groups, int groupcount, wchar_t *commandarguments, size_t *deletiontally, struct status_text *status);
int cmd_delete_selected(struct filegroup *groups, int groupcount, wchar_t *commandarguments, size_t *deletiontally, struct status_text *status);
int cmd_reset_selected(struct filegroup *groups, int groupcount, wchar_t *commandarguments, size_t *deletiontally, struct status_text *status);
