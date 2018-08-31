#ifndef COMMANDS_H
#define COMMANDS_H

struct list;

struct imv_commands {
  struct list *command_list;
};

struct imv_commands *imv_commands_create(void);
void imv_commands_free(struct imv_commands *cmds);
void imv_command_register(struct imv_commands *cmds, const char *command, void (*handler)(struct list*, const char*, void*));
void imv_command_alias(struct imv_commands *cmds, const char *command, const char *alias);
int imv_command_exec(struct imv_commands *cmds, struct list *commands, void *data);

#endif

/* vim:set ts=2 sts=2 sw=2 et: */
