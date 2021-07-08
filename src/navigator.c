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
#include <errno.h>

#include "list.h"

/* Some systems like GNU/Hurd don't define PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct nav_item {
  char *path;
};

struct imv_navigator {
  struct list *paths;
  size_t cur_path;
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
  nav->paths = list_create();
  return nav;
}

void imv_navigator_free(struct imv_navigator *nav)
{
  for (size_t i = 0; i < nav->paths->len; ++i) {
    struct nav_item *nav_item = nav->paths->items[i];
    free(nav_item->path);
  }
  list_deep_free(nav->paths);
  free(nav);
}

static int add_item(struct imv_navigator *nav, const char *path)
{
  struct nav_item *nav_item = calloc(1, sizeof *nav_item);

  nav_item->path = realpath(path, NULL);
  if (!nav_item->path) {
    nav_item->path = strdup(path);
  }

  list_append(nav->paths, nav_item);

  if (nav->paths->len == 1) {
    nav->cur_path = 0;
    nav->changed = 1;
  }

  return 0;
}

int imv_navigator_add(struct imv_navigator *nav, const char *path,
                       int recursive)
{
  char path_buf[PATH_MAX+1];
  struct stat path_info;
  if ((stat(path, &path_info) == 0) &&
      S_ISDIR(path_info.st_mode)) {
    int result = 0;
    struct dirent **dir_list;
    int total_dirs = scandir(path, &dir_list, 0, alphasort);
    if (total_dirs >= 0) {
      for (int i = 0; i < total_dirs; ++i) {
        struct dirent *dir = dir_list[i];
        if (strcmp(dir->d_name, "..") == 0 || strcmp(dir->d_name, ".") == 0) {
          continue;
        }
        snprintf(path_buf, sizeof path_buf, "%s/%s", path, dir->d_name);
        struct stat new_path_info;
        if (stat(path_buf, &new_path_info)) {
          switch (errno) {
          case ELOOP:
          case ENOTDIR:
          case ENOENT: continue;
          default: result = 1; break;
          }
        }
        int is_dir = S_ISDIR(new_path_info.st_mode);
        if (is_dir && recursive) {
          if (imv_navigator_add(nav, path_buf, recursive) != 0) {
            result = 1;
            break;
          }
        } else if (!is_dir) {
          if (add_item(nav, path_buf) != 0) {
            result = 1;
            break;
          }
        }
        free(dir_list[i]);
      }
      free(dir_list);
    }
    return result;
  } else {
    return add_item(nav, path);
  }

  return 0;
}

const char *imv_navigator_selection(struct imv_navigator *nav)
{
  const char *path = imv_navigator_at(nav, nav->cur_path);
  return path ? path : "";
}

size_t imv_navigator_index(struct imv_navigator *nav)
{
  return nav->cur_path;
}

void imv_navigator_select_rel(struct imv_navigator *nav, ssize_t direction)
{
  const ssize_t prev_path = nav->cur_path;
  if (nav->paths->len == 0) {
    return;
  }

  if (direction > 1) {
    direction = div(direction, nav->paths->len).rem;
  } else if (direction < -1) {
    direction = div(direction, nav->paths->len).rem;
  } else if (direction == 0) {
    return;
  }

  ssize_t new_path = nav->cur_path + direction;
  if (new_path >= (ssize_t)nav->paths->len) {
    /* Wrap after the end of the list */
    new_path -= (ssize_t)nav->paths->len;
    nav->wrapped = 1;
  } else if (new_path < 0) {
    /* Wrap before the start of the list */
    new_path += (ssize_t)nav->paths->len;
    nav->wrapped = 1;
  }
  nav->cur_path = (size_t)new_path;
  nav->last_move_direction = direction;
  nav->changed = prev_path != new_path;
  return;
}

void imv_navigator_select_abs(struct imv_navigator *nav, ssize_t index)
{
  /* allow -1 to indicate the last image */
  if (index < 0) {
    index += (ssize_t)nav->paths->len;

    /* but if they go farther back than the first image, stick to first image */
    if (index < 0) {
      index = 0;
    }
  }

  /* stick to last image if we go beyond it */
  if (index >= (ssize_t)nav->paths->len) {
    index = (ssize_t)nav->paths->len - 1;
  }

  const size_t prev_path = nav->cur_path;
  nav->cur_path = (size_t)index;
  nav->changed = prev_path != nav->cur_path;
  nav->last_move_direction = (index >= (ssize_t)prev_path) ? 1 : -1;
}

void imv_navigator_remove(struct imv_navigator *nav, const char *path)
{
  bool found = false;
  size_t removed = 0;
  for (size_t i = 0; i < nav->paths->len; ++i) {
    struct nav_item *item = nav->paths->items[i];
    if (!strcmp(item->path, path)) {
      free(item->path);
      free(item);
      list_remove(nav->paths, i);
      removed = i;
      found = true;
      break;
    }
  }

  if (!found) {
    return;
  }

  if (nav->cur_path == removed) {
    /* We just removed the current path */
    if (nav->last_move_direction < 0) {
      /* Move left */
      imv_navigator_select_rel(nav, -1);
    } else {
      /* Try to stay where we are, unless we ran out of room */
      if (nav->cur_path == nav->paths->len) {
        nav->cur_path = 0;
        nav->wrapped = 1;
      }
    }
  }
  nav->changed = 1;
}

void imv_navigator_remove_at(struct imv_navigator *nav, size_t index)
{
  if (index >= nav->paths->len) {
    return;
  }
  struct nav_item *item = nav->paths->items[index];
  free(item->path);
  free(item);
  list_remove(nav->paths, index);

  if (nav->cur_path == index) {
    /* We just removed the current path */
    if (nav->last_move_direction < 0) {
      /* Move left */
      imv_navigator_select_rel(nav, -1);
    } else {
      /* Try to stay where we are, unless we ran out of room */
      if (nav->cur_path == nav->paths->len) {
        nav->cur_path = 0;
        nav->wrapped = 1;
      }
    }
  }
  nav->changed = 1;
}

void imv_navigator_remove_all(struct imv_navigator *nav)
{
  for (size_t i = 0; i < nav->paths->len; ++i) {
    struct nav_item *item = nav->paths->items[i];
    free(item->path);
    free(item);
  }
  list_clear(nav->paths);
  nav->cur_path = 0;
  nav->changed = 1;
}

ssize_t imv_navigator_find_path(struct imv_navigator *nav, const char *path)
{
  char *real_path = realpath(path, NULL);
  if (real_path) {
    /* first try to match the exact path if path can be resolved */
    for (size_t i = 0; i < nav->paths->len; ++i) {
      struct nav_item *item = nav->paths->items[i];
      if (!strcmp(item->path, real_path)) {
        free(real_path);
        return (ssize_t)i;
      }
    }

    free(real_path);
  }

  /* no exact matches or path cannot be resolved, try the final portion of the path */
  for (size_t i = 0; i < nav->paths->len; ++i) {
    struct nav_item *item = nav->paths->items[i];
    char *last_sep = strrchr(item->path, '/');
    if (last_sep && !strcmp(last_sep+1, path)) {
      return (ssize_t)i;
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

  if (nav->paths->len == 0) {
    return 0;
  };

  time_t cur_time = time(NULL);
  /* limit polling to once per second */
  if (nav->last_check < cur_time - 1) {
    nav->last_check = cur_time;

    struct stat file_info;
    struct nav_item *cur_item = nav->paths->items[nav->cur_path];
    if (stat(cur_item->path, &file_info) == -1) {
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
  return nav->paths->len;
}

char *imv_navigator_at(struct imv_navigator *nav, size_t index)
{
  if (index < nav->paths->len) {
    struct nav_item *item = nav->paths->items[index];
    return item->path;
  }
  return NULL;
}

/* vim:set ts=2 sts=2 sw=2 et: */
