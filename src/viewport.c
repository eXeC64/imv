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

#include "viewport.h"

struct imv_viewport *imv_viewport_create(SDL_Window *window)
{
  struct imv_viewport *view = malloc(sizeof(struct imv_viewport));
  view->window = window;
  view->scale = 1;
  view->x = view->y = view->fullscreen = view->redraw = 0;
  view->playing = 1;
  view->locked = 0;
  return view;
}

void imv_viewport_free(struct imv_viewport *view)
{
  free(view);
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

void imv_viewport_toggle_playing(struct imv_viewport *view)
{
  view->playing = !view->playing;
}

void imv_viewport_scale_to_actual(struct imv_viewport *view, const struct imv_texture *tex)
{
  view->scale = 1;
  view->redraw = 1;
  view->locked = 1;
  imv_viewport_center(view, tex);
}

void imv_viewport_move(struct imv_viewport *view, int x, int y,
    const struct imv_texture *tex)
{
  view->x += x;
  view->y += y;
  view->redraw = 1;
  view->locked = 1;
  int w = (int)((double)tex->width * view->scale);
  int h = (int)((double)tex->height * view->scale);
  int ww, wh;
  SDL_GetWindowSize(view->window, &ww, &wh);
  if (view->x < -w) {
    view->x = -w;
  }
  if (view->x > ww) {
    view->x = ww;
  }
  if (view->y < -h) {
    view->y = -h;
  }
  if (view->y > wh) {
    view->y = wh;
  }
}

void imv_viewport_zoom(struct imv_viewport *view, const struct imv_texture *tex, enum imv_zoom_source src, int amount)
{
  double prev_scale = view->scale;
  int x, y, ww, wh;
  SDL_GetWindowSize(view->window, &ww, &wh);

  /* x and y cordinates are relative to the image */
  if(src == IMV_ZOOM_MOUSE) {
    SDL_GetMouseState(&x, &y);
    x -= view->x;
    y -= view->y;
  } else {
    x = view->scale * tex->width / 2;
    y = view->scale * tex->height / 2;
  }

  const int scaled_width = tex->width * view->scale;
  const int scaled_height = tex->height * view->scale;
  const int ic_x = view->x + scaled_width/2;
  const int ic_y = view->y + scaled_height/2;
  const int wc_x = ww/2;
  const int wc_y = wh/2;

  double delta_scale = 0.04 * ww * amount / tex->width;
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

void imv_viewport_center(struct imv_viewport *view, const struct imv_texture *tex)
{
  int ww, wh;
  SDL_GetWindowSize(view->window, &ww, &wh);

  view->x = (ww - tex->width * view->scale) / 2;
  view->y = (wh - tex->height * view->scale) / 2;

  view->locked = 1;
  view->redraw = 1;
}

void imv_viewport_scale_to_window(struct imv_viewport *view, const struct imv_texture *tex)
{
  int ww, wh;
  SDL_GetWindowSize(view->window, &ww, &wh);

  double window_aspect = (double)ww / (double)wh;
  double image_aspect = (double)tex->width / (double)tex->height;

  if(window_aspect > image_aspect) {
    /* Image will become too tall before it becomes too wide */
    view->scale = (double)wh / (double)tex->height;
  } else {
    /* Image will become too wide before it becomes too tall */
    view->scale = (double)ww / (double)tex->width;
  }

  imv_viewport_center(view, tex);
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

void imv_viewport_update(struct imv_viewport *view, struct imv_texture *tex)
{
  view->redraw = 1;
  if(view->locked) {
    return;
  }

  imv_viewport_center(view, tex);
  imv_viewport_scale_to_window(view, tex);
}

int imv_viewport_needs_redraw(struct imv_viewport *view)
{
  int redraw = 0;
  if(view->redraw) {
    redraw = 1;
    view->redraw = 0;
  }
  return redraw;
}


/* vim:set ts=2 sts=2 sw=2 et: */
