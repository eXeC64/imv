#include "commands.h"
#include "list.h"

struct command {
  char* command;
  void (*handler)(struct list *args, const char *argstr, void *data);
  char* alias;
};

struct imv_commands *imv_commands_create(void)
{
  struct imv_commands *cmds = malloc(sizeof *cmds);
  cmds->command_list = list_create();
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
  list_free(cmds->command_list);
  free(cmds);
}

void imv_command_register(struct imv_commands *cmds, const char *command, void (*handler)(struct list*, const char*, void*))
{
  struct command *cmd = malloc(sizeof *cmd);
  cmd->command = strdup(command);
  cmd->handler = handler;
  cmd->alias = NULL;
  list_append(cmds->command_list, cmd);
}

void imv_command_alias(struct imv_commands *cmds, const char *command, const char *alias)
{
  struct command *cmd = malloc(sizeof *cmd);
  cmd->command = strdup(command);
  cmd->handler = NULL;
  cmd->alias = strdup(alias);
  list_append(cmds->command_list, cmd);
}

int imv_command_exec(struct imv_commands *cmds, const char *command, void *data)
{
  struct list *args = list_from_string(command, ' ');
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

  list_deep_free(args);
  return ret;
}

int imv_command_exec_list(struct imv_commands *cmds, struct list *commands, void *data)
{
  int ret = 0;
  for(size_t i = 0; i < commands->len; ++i) {
    const char *command = commands->items[i];
    ret += imv_command_exec(cmds, command, data);
  }
  return ret;
}

/* vim:set ts=2 sts=2 sw=2 et: */
