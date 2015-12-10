#include <sys/inotify.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "reload.h"

void imv_init_reload(struct imv_reload *rld)
{
  rld->fd = inotify_init1(IN_NONBLOCK);
  if(rld->fd == -1) {
    perror("imv_init_reload");
  }

  rld->wd = 0;
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

void imv_destroy_reload(struct imv_reload *rld)
{
  close(rld->fd);
}
