/* Copyright (c) 2017 imv authors

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

#ifndef IMV_H
#define IMV_H

#include <stdbool.h>

struct imv;

struct imv *imv_create(void);
void imv_free(struct imv *imv);

bool imv_parse_args(struct imv *imv, int argc, char **argv);

void imv_check_stdin_for_paths(struct imv *imv);

void imv_add_path(struct imv *imv, const char *path);

bool imv_run(struct imv *imv);

#endif

/* vim:set ts=2 sts=2 sw=2 et: */
