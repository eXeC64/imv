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

struct imv_command {
  const char* command;
  void (*handler)(struct imv_list *args);
  const char* alias;
};

struct imv_list *g_commands = NULL;

void imv_command_register(const char *command, void (*handler)())
{
  if(!g_commands) {
    g_commands = imv_list_create();
  }

  struct imv_command *cmd = malloc(sizeof(struct imv_command));
  cmd->command = strdup(command);
  cmd->handler = handler;
  cmd->alias = NULL;
  imv_list_append(g_commands, cmd);
}

void imv_command_alias(const char *command, const char *alias)
{
  if(!g_commands) {
    g_commands = imv_list_create();
  }

  struct imv_command *cmd = malloc(sizeof(struct imv_command));
  cmd->command = strdup(command);
  cmd->handler = NULL;
  cmd->alias = strdup(alias);
  imv_list_append(g_commands, cmd);
}

int imv_command_exec(const char *command)
{
  if(!g_commands) {
    return 1;
  }

  struct imv_list *args = imv_split_string(command, ' ');
  int ret = 1;

  if(args->len > 0) {
    for(size_t i = 0; i < g_commands->len; ++i) {
      struct imv_command *cmd = g_commands->items[i];
      if(!strcmp(cmd->command, args->items[0])) {
        if(cmd->handler) {
          cmd->handler(args);
          ret = 0;
        } else if(cmd->alias) {
          ret = imv_command_exec(cmd->alias);
        }
        break;
      }
    }
  }

  imv_list_deep_free(args);
  return ret;
}

/* vim:set ts=2 sts=2 sw=2 et: */
