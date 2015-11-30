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
#include "texture.h"

struct imv_viewport {
  SDL_Window *window;
  double scale;
  int x, y;
  int fullscreen;
  int redraw;
  int playing;
  int locked;
};

/* Used to signify how a a user requested a zoom */
enum imv_zoom_source {
  IMV_ZOOM_MOUSE,
  IMV_ZOOM_KEYBOARD
};

/* Initialises an instance of imv_viewport */
void imv_init_viewport(struct imv_viewport *view, SDL_Window *window);

/* Cleans up all resources owned by a imv_viewport instance */
void imv_destroy_viewport(struct imv_viewport *view);

/* Toggle their viewport's fullscreen mode. Triggers a redraw */
void imv_viewport_toggle_fullscreen(struct imv_viewport *view);

/* Toggle playback of animated gifs */
void imv_viewport_toggle_playing(struct imv_viewport *view);

/* Reset the viewport to its initial settings */
void imv_viewport_reset(struct imv_viewport *view);

/* Pan the view by the given amounts */
void imv_viewport_move(struct imv_viewport *view, int x, int y);

/* Zoom the view by the given amount. imv_texture* is used to get the image
 * dimensions */
void imv_viewport_zoom(struct imv_viewport *view, const struct imv_texture *tex,
                       enum imv_zoom_source, int amount);

/* Recenter the view to be in the middle of the image */
void imv_viewport_center(struct imv_viewport *view,
                         const struct imv_texture *tex);

/* Scale the view so that the image appears at its actual resolution */
void imv_viewport_scale_to_actual(struct imv_viewport *view,
                                  const struct imv_texture *tex);

/* Scale the view so that the image fills the window */
void imv_viewport_scale_to_window(struct imv_viewport *view,
                                  const struct imv_texture *tex);

/* Tell the viewport that it needs to be redrawn */
void imv_viewport_set_redraw(struct imv_viewport *view);

/* Set the title of the viewport */
void imv_viewport_set_title(struct imv_viewport *view, char *title);

/* Tell the viewport the window or image has changed */
void imv_viewport_updated(struct imv_viewport *view, struct imv_texture *tex);

#endif
