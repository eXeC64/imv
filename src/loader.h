#ifndef IMV_LOADER_H
#define IMV_LOADER_H

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
#include <SDL2/SDL.h>
#include <pthread.h>

struct imv_texture;

struct imv_loader {
  pthread_mutex_t lock;
  pthread_t bg_thread;
  char *path;
  FIBITMAP *out_bmp;
  char *out_err;
  FIMULTIBITMAP *mbmp;
  FIBITMAP *bmp;
  int width;
  int height;
  int cur_frame;
  int next_frame;
  int num_frames;
  double frame_time;
};

void imv_init_loader(struct imv_loader *img);
void imv_destroy_loader(struct imv_loader *img);

void imv_loader_load_path(struct imv_loader *ldr, const char *path);

FIBITMAP *imv_loader_get_image(struct imv_loader *ldr);
char *imv_loader_get_error(struct imv_loader *ldr);

/* void imv_loader_next_frame(struct imv_loader *ldr); */
/* void imv_loader_time_passed(struct imv_loader *ldr, double dt); */

#endif
