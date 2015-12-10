#ifndef IMV_RELOAD_H
#define IMV_RELOAD_H

/* Copyright (c) 2015 Jose Diez 

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

struct imv_reload { 
  int fd; // inotify file descriptor
  int wd; // watch descriptor
};

void imv_init_reload(struct imv_reload *rld);
void imv_reload_watch(struct imv_reload *rld, const char *path);
int imv_reload_changed(struct imv_reload *rld);
void imv_destroy_reload(struct imv_reload *rld);

#endif
