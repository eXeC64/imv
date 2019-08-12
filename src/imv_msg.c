#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "ipc.h"

int main(int argc, char **argv)
{
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <pid> <command>\n", argv[0]);
    return 0;
  }

  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  assert(sockfd);

  struct sockaddr_un desc = {
    .sun_family = AF_UNIX
  };
  imv_ipc_path(desc.sun_path, sizeof desc.sun_path, atoi(argv[1]));

  if (connect(sockfd, (struct sockaddr *)&desc, sizeof desc) < 0) {
    perror("Failed to connect");
    return 1;
  }

  char buf[4096] = {0};
  for (int i = 2; i < argc; ++i) {
    strncat(buf, argv[i], sizeof buf - 1);
    if (i + 1 < argc) {
      strncat(buf, " ", sizeof buf - 1);
    }
  }
  strncat(buf, "\n", sizeof buf - 1);

  write(sockfd, buf, strlen(buf));
  close(sockfd);
  return 0;
}
