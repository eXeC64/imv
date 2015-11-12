#ifndef IMV_NAVIGATOR_H
#define IMV_NAVIGATOR_H

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
