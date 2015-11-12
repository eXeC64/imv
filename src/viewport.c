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
#include "image.h"

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

void imv_viewport_toggle_playing(struct imv_viewport *view, struct imv_image *img)
{
  if(view->playing) {
    view->playing = 0;
  } else if(imv_image_is_animated(img)) {
    view->playing = 1;
  }
}

void imv_viewport_scale_to_actual(struct imv_viewport *view, const struct imv_image *img)
{
  view->scale = 1;
  view->redraw = 1;
  view->locked = 1;
  imv_viewport_center(view, img);
}

void imv_viewport_move(struct imv_viewport *view, int x, int y)
{
  view->x += x;
  view->y += y;
  view->redraw = 1;
  view->locked = 1;
}

void imv_viewport_zoom(struct imv_viewport *view, const struct imv_image *img, enum imv_zoom_source src, int amount)
{
  double prevScale = view->scale;
  int x, y, ww, wh;
  SDL_GetWindowSize(view->window, &ww, &wh);

  if(src == MOUSE) {
    SDL_GetMouseState(&x, &y);
    /* Translate mouse coordinates to projected coordinates */
    x -= view->x;
    y -= view->y;
  } else {
    x = 0;
    y = 0;
  }

  const int scaledWidth = img->width * view->scale;
  const int scaledHeight = img->height * view->scale;
  const int ic_x = view->x + scaledWidth/2;
  const int ic_y = view->y + scaledHeight/2;
  const int wc_x = ww/2;
  const int wc_y = wh/2;

  view->scale += (view->scale / img->width) * amount * 20;
  const int min_scale = 0.01;
  const int max_scale = 100;
  if(view->scale > max_scale)
    view->scale = max_scale;
  else if (view->scale < min_scale)
    view->scale = min_scale;

  if(view->scale < prevScale) {
    if(scaledWidth < ww) {
      x = scaledWidth/2 - (ic_x - wc_x)*2;
    }
    if(scaledHeight < wh) {
      y = scaledHeight/2 - (ic_y - wc_y)*2;
    }
  } else {
    if(scaledWidth < ww) {
      x = scaledWidth/2;
    }
    if(scaledHeight < wh) {
      y = scaledHeight/2;
    }
  }

  const double changeX = x - (x * (view->scale / prevScale));
  const double changeY = y - (y * (view->scale / prevScale));

  view->x += changeX;
  view->y += changeY;

  view->redraw = 1;
  view->locked = 1;
}

void imv_viewport_center(struct imv_viewport *view, const struct imv_image* img)
{
  int ww, wh;
  SDL_GetWindowSize(view->window, &ww, &wh);

  view->x = (ww - img->width * view->scale) / 2;
  view->y = (wh - img->height * view->scale) / 2;

  view->locked = 1;
  view->redraw = 1;
}

void imv_viewport_scale_to_window(struct imv_viewport *view, const struct imv_image* img)
{
  int ww, wh;
  SDL_GetWindowSize(view->window, &ww, &wh);

  double window_aspect = (double)ww / (double)wh;
  double image_aspect = (double)img->width / (double)img->height;

  if(window_aspect > image_aspect) {
    /* Image will become too tall before it becomes too wide */
    view->scale = (double)wh / (double)img->height;
  } else {
    /* Image will become too wide before it becomes too tall */
    view->scale = (double)ww / (double)img->width;
  }

  imv_viewport_center(view, img);
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

void imv_viewport_updated(struct imv_viewport *view, struct imv_image* img)
{
  view->redraw = 1;
  if(view->locked) {
    return;
  }

  imv_viewport_scale_to_window(view, img);
  imv_viewport_center(view, img);
}
