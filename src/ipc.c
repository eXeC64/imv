#include "ipc.h"

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct imv_ipc {
  int fd;
  imv_ipc_callback callback;
  void *data;
};

struct connection {
  struct imv_ipc *ipc;
  int fd;
};

static void get_ipc_filename(char *buf, size_t len)
{
  const char *base = getenv("XDG_RUNTIME_DIR");
  if (!base) {
    base = "/tmp";
  }
  snprintf(buf, len, "%s/imv-%d.sock", base, getpid());
}

void *wait_for_commands(void* void_conn)
{
  struct connection *conn = void_conn;

  while (1) {
    char buf[1024];
    ssize_t len = recv(conn->fd, buf, sizeof buf - 1, 0);
    if (len <= 0) {
      break;
    }

    buf[len] = 0;
    while (len > 0 && isspace(buf[len-1])) {
      buf[len-1] = 0;
      --len;
    }

    if (conn->ipc->callback) {
      conn->ipc->callback(buf, conn->ipc->data);
    }
  }

  close(conn->fd);
  free(conn);
  return NULL;
}

void *wait_for_connections(void* void_ipc)
{
  struct imv_ipc *ipc = void_ipc;
  (void)ipc;

  while (1) {
    int client = accept(ipc->fd, NULL, NULL);
    if (client == -1) {
      break;
    }
    struct connection *conn = calloc(1, sizeof *conn);
    conn->ipc = ipc;
    conn->fd = client;

    pthread_t thread;
    pthread_create(&thread, NULL, wait_for_commands, conn);
    pthread_detach(thread);
  }
  return NULL;
}

struct imv_ipc *imv_ipc_create(void)
{
  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd < 0) {
    return NULL;
  }

  struct sockaddr_un desc = {
    .sun_family = AF_UNIX
  };
  get_ipc_filename(desc.sun_path, sizeof desc.sun_path);

  unlink(desc.sun_path);

  if (bind(sockfd, (struct sockaddr*)&desc, sizeof desc) == -1) {
    return NULL;
  }

  if (listen(sockfd, 5) == -1) {
    close(sockfd);
    return NULL;
  }

  struct imv_ipc *ipc = calloc(1, sizeof *ipc);
  ipc->fd = sockfd;

  pthread_t thread;
  pthread_create(&thread, NULL, wait_for_connections, ipc);
  pthread_detach(thread);
  return ipc;
}

void imv_ipc_free(struct imv_ipc *ipc)
{
  if (!ipc) {
    return;
  }

  char ipc_filename[1024];
  get_ipc_filename(ipc_filename, sizeof ipc_filename);
  unlink(ipc_filename);
  close(ipc->fd);

  free(ipc);
}

void imv_ipc_set_command_callback(struct imv_ipc *ipc,
    imv_ipc_callback callback, void *data)
{
  ipc->callback = callback;
  ipc->data = data;
}

