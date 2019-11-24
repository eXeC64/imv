#include <sys/inotify.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "reload.h"

struct imv_reload { 
  int fd; // inotify file descriptor
  int wd; // watch descriptor
};

struct imv_reload *imv_reload_create(void)
{
  struct imv_reload *rld = calloc(1, sizeof *rld);
  rld->fd = inotify_init1(IN_NONBLOCK);
  if(rld->fd == -1) {
    perror("imv_init_reload");
  }

  rld->wd = 0;
  return rld;
}

void imv_reload_watch(struct imv_reload *rld, const char *path)
{
  if(rld->wd != 0) {
    inotify_rm_watch(rld->fd, rld->wd);
  }

  rld->wd = inotify_add_watch(rld->fd, path, IN_CLOSE_WRITE);
  if(rld->wd == -1) {
    perror("imv_reload_watch");
  }
}

int imv_reload_changed(struct imv_reload *rld)
{
  struct inotify_event ev;
  ssize_t len = read(rld->fd, &ev, sizeof(ev));

  if(len < 0) {
    if(errno != EAGAIN) {
      perror("imv_reload_changed");
    }
  } else if(ev.mask & IN_CLOSE_WRITE) {
    return 1;
  }

  return 0;
}

void imv_reload_free(struct imv_reload *rld)
{
  inotify_rm_watch(rld->fd, rld->wd);
  close(rld->fd);
  free(rld);
}
