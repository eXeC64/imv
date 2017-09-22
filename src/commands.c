/* Copyright (c) 2017 imv authors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "commands.h"
#include "list.h"

struct command {
  char* command;
  void (*handler)(struct imv_list *args, const char *argstr, void *data);
  char* alias;
};

struct imv_commands *imv_commands_create(void)
{
  struct imv_commands *cmds = malloc(sizeof(struct imv_commands));
  cmds->command_list = imv_list_create();
  return cmds;
}

void imv_commands_free(struct imv_commands *cmds)
{
  for(size_t i = 0; i < cmds->command_list->len; ++i) {
    struct command *cmd = cmds->command_list->items[i];
    free(cmd->command);
    if(cmd->alias) {
      free(cmd->alias);
    }
    free(cmd);
  }
  imv_list_free(cmds->command_list);
  free(cmds);
}

void imv_command_register(struct imv_commands *cmds, const char *command, void (*handler)(struct imv_list*, const char*, void*))
{
  struct command *cmd = malloc(sizeof(struct command));
  cmd->command = strdup(command);
  cmd->handler = handler;
  cmd->alias = NULL;
  imv_list_append(cmds->command_list, cmd);
}

void imv_command_alias(struct imv_commands *cmds, const char *command, const char *alias)
{
  struct command *cmd = malloc(sizeof(struct command));
  cmd->command = strdup(command);
  cmd->handler = NULL;
  cmd->alias = strdup(alias);
  imv_list_append(cmds->command_list, cmd);
}

int imv_command_exec(struct imv_commands *cmds, const char *command, void *data)
{
  struct imv_list *args = imv_split_string(command, ' ');
  int ret = 1;

  if(args->len > 0) {
    for(size_t i = 0; i < cmds->command_list->len; ++i) {
      struct command *cmd = cmds->command_list->items[i];
      if(!strcmp(cmd->command, args->items[0])) {
        if(cmd->handler) {
          /* argstr = all args as a single string */
          const char *argstr = command + strlen(cmd->command) + 1;
          cmd->handler(args, argstr, data);
          ret = 0;
        } else if(cmd->alias) {
          ret = imv_command_exec(cmds, cmd->alias, data);
        }
        break;
      }
    }
  }

  imv_list_deep_free(args);
  return ret;
}

/* vim:set ts=2 sts=2 sw=2 et: */
