#ifndef IMV_NAVIGATOR_H
#define IMV_NAVIGATOR_H

#include <time.h>

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

#define BUFFER_SIZE 512

struct imv_navigator {
  int num_paths;
  int cur_path;
  char **paths;
  time_t *mtimes;
  int last_move_direction;
  int changed;
  int wrapped;
};

/* Initialises an instance of imv_navigator */
void imv_navigator_init(struct imv_navigator *nav);

/* Cleans up all resources owned by a imv_navigator instance */
void imv_navigator_destroy(struct imv_navigator *nav);

/* Adds the given path to the navigator's internal list.
 * If a directory is given, all files within that directory are added.
 * An internal copy of path is made.
 * If recursive is non-zero then subdirectories are recursed into.
 * Non-zero return code denotes failure. */
int imv_navigator_add(struct imv_navigator *nav, const char *path,
                       int recursive);

/* Returns a read-only reference to the current path. The pointer is only
 * guaranteed to be valid until the next call to an imv_navigator method. */
const char *imv_navigator_selection(struct imv_navigator *nav);

/* Change the currently selected path. dir = -1 for previous, 1 for next. */
void imv_navigator_select_rel(struct imv_navigator *nav, int dir);

/* Change the current selected path.
 * if pos < 0 go to the first image.
 * if pos > num_path go to the last image
 */
void imv_navigator_select_abs(struct imv_navigator *nav, int pos);

/* Removes the given path. The current selection is updated if necessary,
 * based on the last direction the selection moved. */
void imv_navigator_remove(struct imv_navigator *nav, const char *path);

/* Select the given path if it exists. */
void imv_navigator_select_str(struct imv_navigator *nav, const int path);

/* Return the index of the path given. Returns -1 if not found. */
int imv_navigator_find_path(struct imv_navigator *nav, const char *path);

/* Returns 1 if either the currently selected path or underlying file has
 * changed since last called */
int imv_navigator_poll_changed(struct imv_navigator *nav, const int nopoll);

/* Check whether navigator wrapped around paths list */
int imv_navigator_wrapped(struct imv_navigator *nav);

#endif


/* vim:set ts=2 sts=2 sw=2 et: */
