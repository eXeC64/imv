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
  nav->first = NULL;
  nav->last = NULL;
  nav->cur = NULL;
  nav->num_paths = 0;
  nav->last_move_direction = 1;
}

void imv_destroy_navigator(struct imv_navigator *nav)
{
  struct imv_loop_item *next = nav->first;
  while(next) {
    struct imv_loop_item *cur = next;
    next = cur->next;
    free(cur->path);
    free(cur);
  }
  nav->first = NULL;
  nav->last = NULL;
  nav->cur = NULL;
  nav->num_paths = 0;
}

//add a single path item with no other checks
static void add_item(struct imv_navigator *nav, const char *path)
{
  struct imv_loop_item *new_item = (struct imv_loop_item*)
    malloc(sizeof(struct imv_loop_item));
  new_item->path = strdup(path);
  if(!nav->first && !nav->last) {
    nav->first = new_item;
    nav->last = new_item;
    new_item->next = NULL;
    new_item->prev = NULL;
    nav->cur = new_item;
  } else {
    nav->last->next = new_item;
    new_item->prev = nav->last;
    new_item->next = NULL;
    nav->last = new_item;
  }
  nav->num_paths += 1;
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
  return nav->cur->path;
}

void imv_navigator_next_path(struct imv_navigator *nav)
{
  if(nav->num_paths == 0) {
    return;
  }

  if(nav->cur->next) {
    nav->cur = nav->cur->next;
  } else {
    nav->cur = nav->first;
  }
  nav->last_move_direction = 1;
}

void imv_navigator_prev_path(struct imv_navigator *nav)
{
  if(nav->num_paths == 0) {
    return;
  }

  if(nav->cur->prev) {
    nav->cur = nav->cur->prev;
  } else {
    nav->cur = nav->last;
  }

  nav->last_move_direction = -1;
}

void imv_navigator_remove_current_path(struct imv_navigator *nav)
{
  if(nav->num_paths == 0) {
    return;
  }

  nav->num_paths -= 1;

  struct imv_loop_item *cur = nav->cur;
  if(cur->prev) {
    cur->prev->next = cur->next;
  }
  if(cur->next) {
    cur->next->prev = cur->prev;
  }
  if(nav->first == cur) {
    nav->first = cur->next;
  }
  if(nav->last == cur) {
    nav->last = cur->prev;
  }

  if(nav->last_move_direction < 0) {
    imv_navigator_prev_path(nav);
  } else {
    imv_navigator_next_path(nav);
  }

  free(cur->path);
  free(cur);
}
