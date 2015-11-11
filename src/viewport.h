#ifndef IMV_VIEWPORT_H
#define IMV_VIEWPORT_H

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

#include <SDL2/SDL.h>
#include "image.h"

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
  MOUSE,
  KBD
};

void imv_init_viewport(struct imv_viewport *view, SDL_Window *window);
void imv_destroy_viewport(struct imv_viewport *view);

void imv_viewport_toggle_fullscreen(struct imv_viewport*);
void imv_viewport_toggle_playing(struct imv_viewport*, struct imv_image*);
void imv_viewport_reset(struct imv_viewport*);
void imv_viewport_move(struct imv_viewport*, int, int);
void imv_viewport_zoom(struct imv_viewport*, const struct imv_image*, enum imv_zoom_source, int);
void imv_viewport_center(struct imv_viewport*, const struct imv_image*);
void imv_viewport_scale_to_actual(struct imv_viewport*, const struct imv_image*);
void imv_viewport_scale_to_window(struct imv_viewport*, const struct imv_image*);
void imv_viewport_set_redraw(struct imv_viewport*);
void imv_viewport_set_title(struct imv_viewport*, char*);
void imv_viewport_updated(struct imv_viewport*, struct imv_image*);

#endif
