#ifndef IMV_IPC_H
#define IMV_IPC_H

struct imv_ipc;

struct imv_ipc *imv_ipc_create(void);

void imv_ipc_free(struct imv_ipc *ipc);

typedef void (*imv_ipc_callback)(const char *command, void *data);

void imv_ipc_set_command_callback(struct imv_ipc *ipc,
    imv_ipc_callback callback, void *data);

#endif
