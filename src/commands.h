#ifndef COMMANDS_H
#define COMMANDS_H

struct list;
struct imv_commands;

/* Create an imv_commands instance */
struct imv_commands *imv_commands_create(void);

/* Cleans up an imv_commands instance */
void imv_commands_free(struct imv_commands *cmds);

/* Register a new command. When a command is executed, the appropriate handler
 * is called, with a void* for passing context.
 */
void imv_command_register(struct imv_commands *cmds, const char *command,
    void (*handler)(struct list*, const char*, void*));

/* Add a command alias. Any arguments provided when invoking an alias are
 * appended to the arguments being passed to the command.
 */
void imv_command_alias(struct imv_commands *cmds, const char *command, const char *alias);

/* Execute a single command */
int imv_command_exec(struct imv_commands *cmds, const char *command, void *data);

/* Execute a list of commands */
int imv_command_exec_list(struct imv_commands *cmds, struct list *commands, void *data);

#endif

/* vim:set ts=2 sts=2 sw=2 et: */
