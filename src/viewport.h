#ifndef IMV_VIEWPORT_H
#define IMV_VIEWPORT_H

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

#include <SDL2/SDL.h>
#include "loader.h"

struct imv_viewport {
  SDL_Window *window;
  double scale;
  int x, y;
  int fullscreen;
  int redraw;
  int playing;
  int locked;
};

enum imv_zoom_source {
  IMV_ZOOM_MOUSE,
  IMV_ZOOM_KEYBOARD
};

void imv_init_viewport(struct imv_viewport *view, SDL_Window *window);
void imv_destroy_viewport(struct imv_viewport *view);

void imv_viewport_toggle_fullscreen(struct imv_viewport*);
void imv_viewport_toggle_playing(struct imv_viewport*, struct imv_loader*);
void imv_viewport_reset(struct imv_viewport*);
void imv_viewport_move(struct imv_viewport*, int, int);
void imv_viewport_zoom(struct imv_viewport*, const struct imv_loader*, enum imv_zoom_source, int);
void imv_viewport_center(struct imv_viewport*, const struct imv_loader*);
void imv_viewport_scale_to_actual(struct imv_viewport*, const struct imv_loader*);
void imv_viewport_scale_to_window(struct imv_viewport*, const struct imv_loader*);
void imv_viewport_set_redraw(struct imv_viewport*);
void imv_viewport_set_title(struct imv_viewport*, char*);
void imv_viewport_updated(struct imv_viewport*, struct imv_loader*);

#endif
