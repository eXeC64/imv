/* Copyright (c) 2015 Harry Jeffery

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

#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void imv_init_navigator(struct imv_navigator *nav)
{
  nav->buf_size = 512;
  nav->paths = malloc(sizeof(char*) * nav->buf_size);
  nav->num_paths = 0;
  nav->cur_path = 0;
  nav->last_move_direction = 1;
  nav->changed = 0;
}

void imv_destroy_navigator(struct imv_navigator *nav)
{
  if(nav->buf_size > 0) {
    free(nav->paths);
    nav->paths = NULL;
    nav->buf_size = 0;
  }
  nav->num_paths = 0;
}

static void add_item(struct imv_navigator *nav, const char *path)
{
  if(nav->buf_size == nav->num_paths) {
    int new_buf_size = nav->buf_size * 2;
    char **new_paths = malloc(sizeof(char*) * new_buf_size);
    memcpy(new_paths, nav->paths, sizeof(char*) * nav->buf_size);
    free(nav->paths);
    nav->paths = new_paths;
    nav->buf_size = new_buf_size;
  }
  nav->paths[nav->num_paths] = strdup(path);
  nav->num_paths += 1;
  if(nav->num_paths == 1) {
    nav->changed = 1;
  }
}

void imv_navigator_add_path(struct imv_navigator *nav, const char *path)
{
  char path_buf[512];
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
        add_item(nav, path_buf);
      }
      closedir(d);
    }
  } else {
   add_item(nav, path); 
  }
}

void imv_navigator_add_path_recursive(struct imv_navigator *nav, const char *path)
{
  char path_buf[512];
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
        imv_navigator_add_path_recursive(nav, path_buf);
      }
      closedir(d);
    }
  } else {
   add_item(nav, path); 
  }
}

const char *imv_navigator_get_current_path(struct imv_navigator *nav)
{
  if(nav->num_paths == 0) {
    return NULL;
  }
  return nav->paths[nav->cur_path];
}

void imv_navigator_next_path(struct imv_navigator *nav)
{
  int prev_path = nav->cur_path;
  if(nav->num_paths == 0) {
    return;
  }
  nav->cur_path += 1;
  if(nav->cur_path == nav->num_paths) {
    nav->cur_path = 0;
  }
  nav->last_move_direction = 1;
  nav->changed = prev_path != nav->cur_path;
}

void imv_navigator_prev_path(struct imv_navigator *nav)
{
  int prev_path = nav->cur_path;
  if(nav->num_paths == 0) {
    return;
  }
  nav->cur_path -= 1;
  if(nav->cur_path < 0) {
    nav->cur_path = nav->num_paths - 1;
  }
  nav->last_move_direction = -1;
  nav->changed = prev_path != nav->cur_path;
}

void imv_navigator_remove_path(struct imv_navigator *nav, const char *path)
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

  for(int i = removed; i < nav->num_paths - 2; ++i) {
    nav->paths[i] = nav->paths[i+1];
  }

  nav->num_paths -= 1;

  if(nav->cur_path == removed) {
    /* We just removed the current path */
    if(nav->last_move_direction < 0) {
      /* Move left */
      imv_navigator_prev_path(nav);
    } else {
      /* Try to stay where we are, unless we ran out of room */
      if(nav->cur_path == nav->num_paths) {
        nav->cur_path = 0;
      }
    }
  }
}

void imv_navigator_remove_current_path(struct imv_navigator *nav)
{
  if(nav->num_paths == 0) {
    return;
  }

  free(nav->paths[nav->cur_path]);
  for(int i = nav->cur_path; i < nav->num_paths - 1; ++i) {
    nav->paths[i] = nav->paths[i+1];
  }
  nav->num_paths -= 1;

  if(nav->last_move_direction < 0) {
    /* Move left */
    imv_navigator_prev_path(nav);
  } else {
    /* Try to stay where we are, unless we ran out of room */
    if(nav->cur_path == nav->num_paths) {
      nav->cur_path = 0;
    }
  }

  nav->changed = 1;
}

void imv_navigator_set_path(struct imv_navigator *nav, const int path)
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

int imv_navigator_has_changed(struct imv_navigator *nav)
{
  if(nav->changed) {
    nav->changed = 0;
    return 1;
  } else {
    return 0;
  }
}
