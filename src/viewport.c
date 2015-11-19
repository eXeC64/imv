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

#include "viewport.h"

void imv_init_viewport(struct imv_viewport *view, SDL_Window *window)
{
  view->window = window;
  view->scale = 1;
  view->x = view->y = view->fullscreen = view->redraw = 0;
  view->playing = 1;
  view->locked = 0;
}

void imv_destroy_viewport(struct imv_viewport *view)
{
  view->window = NULL;
  return;
}

void imv_viewport_toggle_fullscreen(struct imv_viewport *view)
{
  if(view->fullscreen) {
    SDL_SetWindowFullscreen(view->window, 0);
    view->fullscreen = 0;
  } else {
    SDL_SetWindowFullscreen(view->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    view->fullscreen = 1;
  }
}

void imv_viewport_toggle_playing(struct imv_viewport *view, struct imv_loader *ldr)
{
  if(view->playing) {
    view->playing = 0;
  } else if(imv_loader_is_animated(ldr)) {
    view->playing = 1;
  }
}

void imv_viewport_scale_to_actual(struct imv_viewport *view, const struct imv_loader *ldr)
{
  view->scale = 1;
  view->redraw = 1;
  view->locked = 1;
  imv_viewport_center(view, ldr);
}

void imv_viewport_move(struct imv_viewport *view, int x, int y)
{
  view->x += x;
  view->y += y;
  view->redraw = 1;
  view->locked = 1;
}

void imv_viewport_zoom(struct imv_viewport *view, const struct imv_loader *ldr, enum imv_zoom_source src, int amount)
{
  double prev_scale = view->scale;
  int x, y, ww, wh;
  SDL_GetWindowSize(view->window, &ww, &wh);

  /* x and y cordinates are relative to the loader */
  if(src == IMV_ZOOM_MOUSE) {
    SDL_GetMouseState(&x, &y);
    x -= view->x;
    y -= view->y;
  } else {
    x = view->scale * ldr->width / 2;
    y = view->scale * ldr->height / 2;
  }

  const int scaled_width = ldr->width * view->scale;
  const int scaled_height = ldr->height * view->scale;
  const int ic_x = view->x + scaled_width/2;
  const int ic_y = view->y + scaled_height/2;
  const int wc_x = ww/2;
  const int wc_y = wh/2;

  double delta_scale = 0.04 * ww * amount / ldr->width;
  view->scale += delta_scale;

  const double min_scale = 0.1;
  const double max_scale = 100;
  if(view->scale > max_scale) {
    view->scale = max_scale;
  } else if (view->scale < min_scale) {
    view->scale = min_scale;
  }

  if(view->scale < prev_scale) {
    if(scaled_width < ww) {
      x = scaled_width/2 - (ic_x - wc_x)*2;
    }
    if(scaled_height < wh) {
      y = scaled_height/2 - (ic_y - wc_y)*2;
    }
  } else {
    if(scaled_width < ww) {
      x = scaled_width/2;
    }
    if(scaled_height < wh) {
      y = scaled_height/2;
    }
  }

  const double changeX = x - (x * (view->scale / prev_scale));
  const double changeY = y - (y * (view->scale / prev_scale));

  view->x += changeX;
  view->y += changeY;

  view->redraw = 1;
  view->locked = 1;
}

void imv_viewport_center(struct imv_viewport *view, const struct imv_loader* ldr)
{
  int ww, wh;
  SDL_GetWindowSize(view->window, &ww, &wh);

  view->x = (ww - ldr->width * view->scale) / 2;
  view->y = (wh - ldr->height * view->scale) / 2;

  view->locked = 1;
  view->redraw = 1;
}

void imv_viewport_scale_to_window(struct imv_viewport *view, const struct imv_loader* ldr)
{
  int ww, wh;
  SDL_GetWindowSize(view->window, &ww, &wh);

  double window_aspect = (double)ww / (double)wh;
  double image_aspect = (double)ldr->width / (double)ldr->height;

  if(window_aspect > image_aspect) {
    /* Image will become too tall before it becomes too wide */
    view->scale = (double)wh / (double)ldr->height;
  } else {
    /* Image will become too wide before it becomes too tall */
    view->scale = (double)ww / (double)ldr->width;
  }

  imv_viewport_center(view, ldr);
  view->locked = 0;
}

void imv_viewport_set_redraw(struct imv_viewport *view)
{
  view->redraw = 1;
}

void imv_viewport_set_title(struct imv_viewport *view, char* title)
{
  SDL_SetWindowTitle(view->window, title);
}

void imv_viewport_updated(struct imv_viewport *view, struct imv_loader* ldr)
{
  view->redraw = 1;
  if(view->locked) {
    return;
  }

  imv_viewport_scale_to_window(view, ldr);
  imv_viewport_center(view, ldr);
}
