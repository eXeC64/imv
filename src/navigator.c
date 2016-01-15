/* Copyright (c) 2015 imv authors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "navigator.h"

#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void imv_navigator_init(struct imv_navigator *nav)
{
  nav->buf_size = 512;
  nav->paths = malloc(sizeof(char*) * nav->buf_size);
  if (nav->paths == NULL) {
    perror("imv_navigator_init");
    exit(1);
  }
  nav->mtimes = malloc(sizeof(time_t) * nav->buf_size);
  if (nav->paths == NULL) {
    perror("imv_navigator_init");
    free(nav->paths);
    exit(1);
  }
  nav->num_paths = 0;
  nav->cur_path = 0;
  nav->last_move_direction = 1;
  nav->changed = 0;
}

void imv_navigator_destroy(struct imv_navigator *nav)
{
  if(nav->buf_size > 0) {
    free(nav->paths);
    nav->paths = NULL;
    free(nav->mtimes);
    nav->mtimes = NULL;
    nav->buf_size = 0;
  }
  nav->num_paths = 0;
}

static void add_item(struct imv_navigator *nav, const char *path,
                     time_t mtime)
{
  if(nav->buf_size == nav->num_paths) {
    char **new_paths;
    time_t *new_mtimes;
    nav->buf_size *= 2;
    new_paths = realloc(nav->paths, sizeof(char*) * nav->buf_size);
    new_mtimes = realloc(nav->mtimes, sizeof(time_t) * nav->buf_size);
    if (new_paths == NULL || new_mtimes == NULL) {
      perror("add_item");
      free(nav->paths);
      free(nav->mtimes);
      exit(1);
    }
    nav->paths = new_paths;
    nav->mtimes = new_mtimes;
  }
  nav->paths[nav->num_paths] = strdup(path);
  nav->mtimes[nav->num_paths] = mtime;
  nav->num_paths += 1;
  if(nav->num_paths == 1) {
    nav->changed = 1;
  }
}

void imv_navigator_add(struct imv_navigator *nav, const char *path,
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
        if(strcmp(dir->d_name, "..") == 0 ||
            strcmp(dir->d_name, ".") == 0) {
          continue;
        }
        snprintf(path_buf, sizeof(path_buf), "%s/%s", path, dir->d_name);
        if(recursive) {
          imv_navigator_add(nav, path_buf, recursive);
        } else {
          add_item(nav, path_buf, path_info.st_mtim.tv_sec);
        }
      }
      closedir(d);
    }
  } else {
   add_item(nav, path, path_info.st_mtim.tv_sec); 
  }
}

const char *imv_navigator_selection(struct imv_navigator *nav)
{
  if(nav->num_paths == 0) {
    return NULL;
  }
  return nav->paths[nav->cur_path];
}

void imv_navigator_select_rel(struct imv_navigator *nav, int direction)
{
  int prev_path = nav->cur_path;
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
    nav->cur_path = 0;
  } else if(nav->cur_path < 0) {
    nav->cur_path = nav->num_paths - 1;
  }
  nav->last_move_direction = direction;
  nav->changed = prev_path != nav->cur_path;
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

int imv_navigator_poll_changed(struct imv_navigator *nav, const int nopoll)
{
  if(nav->changed) {
    nav->changed = 0;
    return 1;
  }
  
  if(!nopoll) {
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
