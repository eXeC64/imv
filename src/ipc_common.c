#include "ipc.h"

#include <stdio.h>
#include <stdlib.h>

void imv_ipc_path(char *buf, size_t len, int pid)
{
  const char *base = getenv("XDG_RUNTIME_DIR");
  if (!base) {
    base = "/tmp";
  }
  snprintf(buf, len, "%s/imv-%d.sock", base, pid);
}

