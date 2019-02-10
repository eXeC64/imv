#include "navigator.h"

#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Some systems like GNU/Hurd don't define PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct imv_navigator {
  int num_paths;
  int cur_path;
  char **paths;
  time_t *mtimes;
  time_t *ctimes;
  int last_move_direction;
  int changed;
  int wrapped;
  int poll_countdown;
};

struct imv_navigator *imv_navigator_create(void)
{
  struct imv_navigator *nav = malloc(sizeof *nav);
  memset(nav, 0, sizeof(struct imv_navigator));
  nav->last_move_direction = 1;
  return nav;
}

void imv_navigator_free(struct imv_navigator *nav)
{
  if(nav->paths) {
    for(int i = 0; i < nav->num_paths; ++i) {
      if(nav->paths[i] != NULL) {
        free(nav->paths[i]);
      }
    }
    free(nav->paths);
  }

  if(nav->mtimes) {
    free(nav->mtimes);
  }

  if(nav->ctimes) {
    free(nav->ctimes);
  }

  free(nav);
}

static int add_item(struct imv_navigator *nav, const char *path,
                     time_t mtime)
{
  const size_t buf_size = 512;

  if(nav->num_paths % buf_size == 0) {
    char **new_paths;
    time_t *new_mtimes;
    time_t *new_ctimes;
    size_t new_size = nav->num_paths + buf_size;
    new_paths = realloc(nav->paths, sizeof(char*) * new_size);
    new_mtimes = realloc(nav->mtimes, sizeof(time_t) * new_size);
    new_ctimes = realloc(nav->ctimes, sizeof(time_t) * new_size);
    if (new_paths == NULL || new_mtimes == NULL || new_ctimes == NULL) {
      return 1;
    }
    nav->paths = new_paths;
    nav->mtimes = new_mtimes;
    nav->ctimes = new_ctimes;
  }
  if((nav->paths[nav->num_paths] = strndup(path, PATH_MAX)) == NULL) {
    return 1;
  }
  nav->mtimes[nav->num_paths] = mtime;
  nav->ctimes[nav->num_paths] = time(NULL);
  nav->num_paths += 1;
  if(nav->num_paths == 1) {
    nav->changed = 1;
  }

  return 0;
}

int imv_navigator_add(struct imv_navigator *nav, const char *path,
                       int recursive)
{
  char path_buf[PATH_MAX];
  struct stat path_info;
  stat(path, &path_info);
  if(S_ISDIR(path_info.st_mode)) {
    DIR *d = opendir(path);
    if(d) {
      struct dirent *dir;
      while((dir = readdir(d)) != NULL) {
        if(strcmp(dir->d_name, "..") == 0 || strcmp(dir->d_name, ".") == 0) {
          continue;
        }
        snprintf(path_buf, sizeof(path_buf), "%s/%s", path, dir->d_name);
        if(recursive) {
          if(imv_navigator_add(nav, path_buf, recursive) != 0) {
            return 1;
          }
        } else {
          stat(path_buf, &path_info);
          if(add_item(nav, path_buf, path_info.st_mtim.tv_sec) != 0) {
            return 1;
          }
        }
      }
      closedir(d);
    }
  } else {
    return add_item(nav, path, path_info.st_mtim.tv_sec); 
  }

  return 0;
}

const char *imv_navigator_selection(struct imv_navigator *nav)
{
  if(nav->num_paths == 0) {
    return "";
  }
  return nav->paths[nav->cur_path];
}

size_t imv_navigator_index(struct imv_navigator *nav)
{
  return (size_t)nav->cur_path;
}

void imv_navigator_select_rel(struct imv_navigator *nav, int direction)
{
  const int prev_path = nav->cur_path;
  if(nav->num_paths == 0) {
    return;
  }

  if(direction > 1) {
    direction = 1;
  } else if(direction < -1) {
    direction = -1;
  } else if(direction == 0) {
    return;
  }

  nav->cur_path += direction;
  if(nav->cur_path == nav->num_paths) {
    /* Wrap after the end of the list */
    nav->cur_path = 0;
    nav->wrapped = 1;
  } else if(nav->cur_path < 0) {
    /* Wrap before the start of the list */
    nav->cur_path = nav->num_paths - 1;
    nav->wrapped = 1;
  }
  nav->last_move_direction = direction;
  nav->changed = prev_path != nav->cur_path;
  return;
}

void imv_navigator_select_abs(struct imv_navigator *nav, int index)
{
  const int prev_path = nav->cur_path;
  /* allow -1 to indicate the last image */
  if(index < 0) {
    index += nav->num_paths;

    /* but if they go farther back than the first image, stick to first image */
    if(index < 0) {
      index = 0;
    }
  }

  /* stick to last image if we go beyond it */
  if(index >= nav->num_paths) {
    index = nav->num_paths - 1;
  }

  nav->cur_path = index;
  nav->changed = prev_path != nav->cur_path;
  nav->last_move_direction = (index >= prev_path) ? 1 : -1;
}

void imv_navigator_remove(struct imv_navigator *nav, const char *path)
{
  int removed = -1;
  for(int i = 0; i < nav->num_paths; ++i) {
    if(strcmp(path, nav->paths[i]) == 0) {
      removed = i;
      free(nav->paths[i]);
      break;
    }
  }

  if(removed == -1) {
    return;
  }

  for(int i = removed; i < nav->num_paths - 1; ++i) {
    nav->paths[i] = nav->paths[i+1];
  }

  nav->num_paths -= 1;

  if(nav->cur_path == removed) {
    /* We just removed the current path */
    if(nav->last_move_direction < 0) {
      /* Move left */
      imv_navigator_select_rel(nav, -1);
    } else {
      /* Try to stay where we are, unless we ran out of room */
      if(nav->cur_path == nav->num_paths) {
        nav->cur_path = 0;
        nav->wrapped = 1;
      }
    }
  }
  nav->changed = 1;
}

void imv_navigator_select_str(struct imv_navigator *nav, const int path)
{
  if(path <= 0 || path >= nav->num_paths) {
    return;
  }
  int prev_path = nav->cur_path;
  nav->cur_path = path;
  nav->changed = prev_path != nav->cur_path;
}

int imv_navigator_find_path(struct imv_navigator *nav, const char *path)
{
  /* first try to match the exact path */
  for(int i = 0; i < nav->num_paths; ++i) {
    if(strcmp(path, nav->paths[i]) == 0) {
      return i;
    }
  }

  /* no exact matches, try the final portion of the path */
  for(int i = 0; i < nav->num_paths; ++i) {
    char *last_sep = strrchr(nav->paths[i], '/');
    if(last_sep && strcmp(last_sep+1, path) == 0) {
      return i;
    }
  }

  /* no matches at all, give up */
  return -1;
}

int imv_navigator_poll_changed(struct imv_navigator *nav)
{
  if(nav->changed) {
    nav->changed = 0;
    return 1;
  }
  
  if(nav->paths == NULL) {
    return 0;
  };

  time_t cur_time = time(NULL);
  /* limit polling to once per second */
  if(nav->ctimes[nav->cur_path] < cur_time - 1) {
    nav->ctimes[nav->cur_path] = cur_time;

    struct stat file_info;
    if(stat(nav->paths[nav->cur_path], &file_info) == -1) {
      return 0;
    }
    if(nav->mtimes[nav->cur_path] != file_info.st_mtim.tv_sec) {
      nav->mtimes[nav->cur_path] = file_info.st_mtim.tv_sec;
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
  return (size_t)nav->num_paths;
}

char *imv_navigator_at(struct imv_navigator *nav, int index)
{
  if(index >= 0 && index < nav->num_paths) {
    return nav->paths[index];
  }
  return NULL;
}

/* vim:set ts=2 sts=2 sw=2 et: */
