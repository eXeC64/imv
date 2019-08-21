#ifndef IMV_IPC_H
#define IMV_IPC_H

#include <unistd.h>

/* imv_ipc provides a listener on a unix socket that listens for commands.
 * When a command is received, a callback function is called.
 */
struct imv_ipc;

/* Creates an imv_ipc instance */
struct imv_ipc *imv_ipc_create(void);

/* Cleans up an imv_ipc instance */
void imv_ipc_free(struct imv_ipc *ipc);

typedef void (*imv_ipc_callback)(const char *command, void *data);

/* When a command is received, imv_ipc will call the callback function passed
 * in. Only one callback function at a time can be connected. The data argument
 * is passed back to the callback to allow for context passing
 */
void imv_ipc_set_command_callback(struct imv_ipc *ipc,
    imv_ipc_callback callback, void *data);

/* Given a pid, emits the path of the unix socket that would connect to an imv
 * instance with that pid
 */
void imv_ipc_path(char *buf, size_t len, int pid);

#endif
