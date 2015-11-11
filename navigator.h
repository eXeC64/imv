#ifndef IMV_NAVIGATOR_H
#define IMV_NAVIGATOR_H

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

struct imv_navigator {
  int num_paths;
  int buf_size;
  int cur_path;
  char **paths;
  int last_move_direction;
  int changed;
};

void imv_init_navigator(struct imv_navigator *nav);
void imv_destroy_navigator(struct imv_navigator *nav);

void imv_navigator_add_path(struct imv_navigator *nav, const char *path);
void imv_navigator_add_path_recursive(struct imv_navigator *nav, const char *path);

const char *imv_navigator_get_current_path(struct imv_navigator *nav);
void imv_navigator_next_path(struct imv_navigator *nav);
void imv_navigator_prev_path(struct imv_navigator *nav);
void imv_navigator_remove_current_path(struct imv_navigator *nav);

int imv_navigator_has_changed(struct imv_navigator *nav);

#endif
