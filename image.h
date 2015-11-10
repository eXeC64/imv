#ifndef IMV_IMAGE_H
#define IMV_IMAGE_H

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

#include <FreeImage.h>

struct imv_image {
  FIMULTIBITMAP *mbmp;
  FIBITMAP *cur_bmp;
  int width;
  int height;
  int cur_frame;
  int next_frame;
  int num_frames;
  double frame_time;
};

void imv_init_image(struct imv_image *img);
void imv_destroy_image(struct imv_image *img);

int imv_can_load_image(const char* path);
int imv_image_load(struct imv_image *img, const char* path);
void imv_image_load_next_frame(struct imv_image *img);

int imv_image_is_animated(struct imv_image *img);
void imv_image_play(struct imv_image *img, double time);

#endif
