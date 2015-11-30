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

/* Initialises an instance of imv_navigator */
void imv_init_navigator(struct imv_navigator *nav);

/* Cleans up all resources owned by a imv_navigator instance */
void imv_destroy_navigator(struct imv_navigator *nav);

/* Adds the given path to the navigator's internal list.
 * If a directory is given, all files within that directory are added.
 * An internal copy of path is made. */
void imv_navigator_add_path(struct imv_navigator *nav, const char *path);

/* Adds the given path to the navigator's internal list.
 * If a directory is given, all files within that directory are added.
 * Subdirectories are recursed into. An internal copy of path is made. */
void imv_navigator_add_path_recursive(struct imv_navigator *nav, const char *path);

/* Returns a read-only reference to the current path. The pointer is only
 * guaranteed to be valid until the next call to and imv_navigator method.
 */
const char *imv_navigator_get_current_path(struct imv_navigator *nav);

/* Changes to the next path */
void imv_navigator_next_path(struct imv_navigator *nav);

/* Changes to the previous path */
void imv_navigator_prev_path(struct imv_navigator *nav);

/* Removes the currently selected path. The current selection is updated if
 * necessary, based on the last direction the selection moved. */
void imv_navigator_remove_current_path(struct imv_navigator *nav);

/* Removes the given path if it exists. The current selection is updated if
 * necessary, based on the last direction the selection moved. */
void imv_navigator_remove_path(struct imv_navigator *nav, const char *path);

/* Set the current path to the given path if it exists. */
void imv_navigator_set_path(struct imv_navigator *nav, const int path);

/* Return the index of the path given. Returns -1 if not found. */
int imv_navigator_find_path(struct imv_navigator *nav, const char *path);

/* Returns 1 if the currently selected path has changed since last called */
int imv_navigator_has_changed(struct imv_navigator *nav);

#endif
