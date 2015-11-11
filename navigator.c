/* Copyright (c) 2015 Harry Jeffery

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "navigator.h"

#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void imv_init_navigator(struct imv_navigator *nav)
{
  nav->buf_size = 512;
  nav->paths = (char **)malloc(sizeof(char*) * nav->buf_size);
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

int imv_can_load_image(const char* path);

static void add_item(struct imv_navigator *nav, const char *path)
{
  if(!imv_can_load_image(path)) {
    return;
  }

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
  if(nav->num_paths == 0) {
    return;
  }
  nav->cur_path += 1;
  if(nav->cur_path == nav->num_paths) {
    nav->cur_path = 0;
  }
  nav->last_move_direction = 1;
  nav->changed = 1;
}

void imv_navigator_prev_path(struct imv_navigator *nav)
{
  if(nav->num_paths == 0) {
    return;
  }
  nav->cur_path -= 1;
  if(nav->cur_path < 0) {
    nav->cur_path = nav->num_paths - 1;
  }
  nav->last_move_direction = -1;
  nav->changed = 1;
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

int imv_navigator_has_changed(struct imv_navigator *nav)
{
  if(nav->changed) {
    nav->changed = 0;
    return 1;
  } else {
    return 0;
  }
}
