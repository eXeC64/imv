#include "navigator.h"

#include <assert.h>
#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* Some systems like GNU/Hurd don't define PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct path {
  char *path;
};

struct imv_navigator {
  struct {
    struct path *items;
    ssize_t len;
    ssize_t cap;
  } paths;
  ssize_t cur_path;
  time_t last_change;
  time_t last_check;
  int last_move_direction;
  int changed;
  int wrapped;
};

struct imv_navigator *imv_navigator_create(void)
{
  struct imv_navigator *nav = calloc(1, sizeof *nav);
  nav->last_move_direction = 1;
  nav->paths.cap = 128;
  nav->paths.items = malloc(nav->paths.cap * sizeof *nav->paths.items);
  return nav;
}

void imv_navigator_free(struct imv_navigator *nav)
{
  for (ssize_t i = 0; i < nav->paths.len; ++i) {
    struct path *path = &nav->paths.items[i];
    free(path->path);
  }
  free(nav->paths.items);
  free(nav);
}

static int add_item(struct imv_navigator *nav, const char *path)
{
  if (nav->paths.cap == nav->paths.len) {
    ssize_t new_cap = nav->paths.cap * 2;
    struct path *new_paths = realloc(nav->paths.items, new_cap * sizeof *nav->paths.items);
    assert(new_paths);
    nav->paths.items = new_paths;
  }

  char *raw_path = realpath(path, NULL);
  if (!raw_path) {
    return 1;
  }

  struct path *new_path = &nav->paths.items[nav->paths.len++];
  memset(new_path, 0, sizeof *new_path);
  new_path->path = strdup(path);

  if (nav->paths.len == 1) {
    nav->changed = 1;
  }

  return 0;
}

int imv_navigator_add(struct imv_navigator *nav, const char *path,
                       int recursive)
{
  char path_buf[PATH_MAX+1];
  struct stat path_info;
  stat(path, &path_info);
  if (S_ISDIR(path_info.st_mode)) {
    DIR *d = opendir(path);
    if (d) {
      struct dirent *dir;
      while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, "..") == 0 || strcmp(dir->d_name, ".") == 0) {
          continue;
        }
        snprintf(path_buf, sizeof path_buf, "%s/%s", path, dir->d_name);
        if (recursive) {
          if (imv_navigator_add(nav, path_buf, recursive) != 0) {
            return 1;
          }
        } else {
          if (add_item(nav, path_buf) != 0) {
            return 1;
          }
        }
      }
      closedir(d);
    }
  } else {
    return add_item(nav, path);
  }

  return 0;
}

const char *imv_navigator_selection(struct imv_navigator *nav)
{
  if (nav->paths.len == 0) {
    return "";
  }
  return nav->paths.items[nav->cur_path].path;
}

size_t imv_navigator_index(struct imv_navigator *nav)
{
  return (size_t)nav->cur_path;
}

void imv_navigator_select_rel(struct imv_navigator *nav, int direction)
{
  const ssize_t prev_path = nav->cur_path;
  if (nav->paths.len == 0) {
    return;
  }

  if (direction > 1) {
    direction = direction % nav->paths.len;
  } else if (direction < -1) {
    direction = direction % nav->paths.len;
  } else if (direction == 0) {
    return;
  }

  nav->cur_path += direction;
  if (nav->cur_path >= nav->paths.len) {
    /* Wrap after the end of the list */
    nav->cur_path = nav->cur_path - nav->paths.len;
    nav->wrapped = 1;
  } else if (nav->cur_path < 0) {
    /* Wrap before the start of the list */
    nav->cur_path = nav->paths.len + nav->cur_path;
    nav->wrapped = 1;
  }
  nav->last_move_direction = direction;
  nav->changed = prev_path != nav->cur_path;
  return;
}

void imv_navigator_select_abs(struct imv_navigator *nav, int index)
{
  const ssize_t prev_path = nav->cur_path;
  /* allow -1 to indicate the last image */
  if (index < 0) {
    index += nav->paths.len;

    /* but if they go farther back than the first image, stick to first image */
    if (index < 0) {
      index = 0;
    }
  }

  /* stick to last image if we go beyond it */
  if (index >= nav->paths.len) {
    index = nav->paths.len - 1;
  }

  nav->cur_path = index;
  nav->changed = prev_path != nav->cur_path;
  nav->last_move_direction = (index >= prev_path) ? 1 : -1;
}

void imv_navigator_remove(struct imv_navigator *nav, const char *path)
{
  ssize_t removed = -1;
  for (ssize_t i = 0; i < nav->paths.len; ++i) {
    if (!strcmp(path, nav->paths.items[i].path)) {
      removed = i;
      free(nav->paths.items[i].path);
      break;
    }
  }

  if (removed == -1) {
    return;
  }

  for (ssize_t i = removed; i < nav->paths.len - 1; ++i) {
    nav->paths.items[i] = nav->paths.items[i+1];
  }

  nav->paths.len -= 1;

  if (nav->cur_path == removed) {
    /* We just removed the current path */
    if (nav->last_move_direction < 0) {
      /* Move left */
      imv_navigator_select_rel(nav, -1);
    } else {
      /* Try to stay where we are, unless we ran out of room */
      if (nav->cur_path == nav->paths.len) {
        nav->cur_path = 0;
        nav->wrapped = 1;
      }
    }
  }
  nav->changed = 1;
}

void imv_navigator_select_str(struct imv_navigator *nav, const int path)
{
  if (path <= 0 || path >= nav->paths.len) {
    return;
  }
  ssize_t prev_path = nav->cur_path;
  nav->cur_path = path;
  nav->changed = prev_path != nav->cur_path;
}

int imv_navigator_find_path(struct imv_navigator *nav, const char *path)
{
  /* first try to match the exact path */
  for (ssize_t i = 0; i < nav->paths.len; ++i) {
    if (!strcmp(path, nav->paths.items[i].path)) {
      return i;
    }
  }

  /* no exact matches, try the final portion of the path */
  for (ssize_t i = 0; i < nav->paths.len; ++i) {
    char *last_sep = strrchr(nav->paths.items[i].path, '/');
    if (last_sep && strcmp(last_sep+1, path) == 0) {
      return i;
    }
  }

  /* no matches at all, give up */
  return -1;
}

int imv_navigator_poll_changed(struct imv_navigator *nav)
{
  if (nav->changed) {
    nav->changed = 0;
    nav->last_change = time(NULL);
    return 1;
  }

  if (nav->paths.len == 0) {
    return 0;
  };

  time_t cur_time = time(NULL);
  /* limit polling to once per second */
  if (nav->last_check < cur_time - 1) {
    nav->last_check = cur_time;

    struct stat file_info;
    if (stat(nav->paths.items[nav->cur_path].path, &file_info) == -1) {
      return 0;
    }

    time_t file_changed = file_info.st_mtim.tv_sec;
    if (file_changed > nav->last_change) {
      nav->last_change = file_changed;
      return 1;
    }
  }
  return 0;
}

int imv_navigator_wrapped(struct imv_navigator *nav)
{
  return nav->wrapped;
}

size_t imv_navigator_length(struct imv_navigator *nav)
{
  return (size_t)nav->paths.len;
}

char *imv_navigator_at(struct imv_navigator *nav, int index)
{
  if (index >= 0 && index < nav->paths.len) {
    return nav->paths.items[index].path;
  }
  return NULL;
}

/* vim:set ts=2 sts=2 sw=2 et: */
