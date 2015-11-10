/* Copyright (c) 2015 Harry Jeffery

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "viewport.h"
#include "image.h"

void imv_init_viewport(struct imv_viewport *view, SDL_Window *window)
{
  view->window = window;
  view->scale = 1;
  view->x = view->y = view->fullscreen = view->redraw = 0;
  view->playing = 1;
}

void imv_destroy_viewport(struct imv_viewport *view)
{
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

void imv_viewport_reset(struct imv_viewport *view)
{
  view->scale = 1;
  view->x = view->y = 0;
  view->redraw = 1;
}

void imv_viewport_move(struct imv_viewport *view, int x, int y)
{
  view->x += x;
  view->y += y;
  view->redraw = 1;
}

void imv_viewport_zoom(struct imv_viewport *view, int amount)
{
  view->scale += amount * 0.1;
  if(view->scale > 100)
    view->scale = 10;
  else if (view->scale < 0.01)
    view->scale = 0.1;
  view->redraw = 1;
}

void imv_viewport_center(struct imv_viewport *view, const struct imv_image* img)
{
  int ww, wh;
  SDL_GetWindowSize(view->window, &ww, &wh);

  view->x = (ww - img->width * view->scale) / 2;
  view->y = (wh - img->height * view->scale) / 2;

  view->redraw = 1;
}

void imv_viewport_scale_to_window(struct imv_viewport *view, const struct imv_image* img)
{
  int ww, wh;
  SDL_GetWindowSize(view->window, &ww, &wh);

  double window_aspect = (double)ww / (double)wh;
  double image_aspect = (double)img->width / (double)img->height;

  if(window_aspect > image_aspect) {
    //Image will become too tall before it becomes too wide
    view->scale = (double)wh / (double)img->height;
  } else {
    //Image will become too wide before it becomes too tall
    view->scale = (double)ww / (double)img->width;
  }

  imv_viewport_center(view, img);
}

void imv_viewport_set_redraw(struct imv_viewport *view)
{
  view->redraw = 1;
}
