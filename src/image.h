#ifndef IMV_IMAGE_H
#define IMV_IMAGE_H

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

#include <FreeImage.h>

struct imv_image {
  FIMULTIBITMAP *mbmp;
  FIBITMAP *cur_bmp;
  int width;
  int height;
  int cur_frame;
  int next_frame;
  int num_frames;
  int changed;
  double frame_time;
};

void imv_init_image(struct imv_image *img);
void imv_destroy_image(struct imv_image *img);

int imv_can_load_image(const char* path);
int imv_image_load(struct imv_image *img, const char* path);
void imv_image_load_next_frame(struct imv_image *img);

int imv_image_is_animated(struct imv_image *img);
void imv_image_play(struct imv_image *img, double time);

int imv_image_has_changed(struct imv_image *img);

#endif
