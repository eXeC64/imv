#ifndef IMV_LOADER_H
#define IMV_LOADER_H

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

#include <FreeImage.h>
#include <SDL2/SDL.h>
#include <pthread.h>

struct imv_texture;

struct imv_loader {
  pthread_mutex_t lock;
  pthread_t bg_thread;
  char *path;
  BYTE *buffer;
  size_t buffer_size;
  FIMEMORY *fi_buffer;
  FIBITMAP *out_bmp;
  int frame_number;
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

/* Initialises an instance of imv_loader */
void imv_init_loader(struct imv_loader *img);

/* Cleans up all resources owned by a imv_loader instance */
void imv_destroy_loader(struct imv_loader *img);

/* Asynchronously load the given file */
void imv_loader_load(struct imv_loader *ldr, const char *path,
                     const void *buffer, const size_t buffer_size);

/* Returns 1 if image data is available. 0 if not. Caller is responsible for
 * cleaning up the data returned. Each image is only returned once.
 * frame_number and num_frames report number of current frame and total amount
 * of frames in image. */
int imv_loader_get_image(struct imv_loader *ldr, FIBITMAP **out_bmp,
                         int *frame_number, int *num_frames);

/* If a file failed to load, return the path to that file. Otherwise returns
 * NULL. Only returns the path once. Caller is responsible for cleaning up the
 * string returned. */
char *imv_loader_get_error(struct imv_loader *ldr);

/* Trigger the next frame of the currently loaded image to be loaded and
 * returned as soon as possible. */
void imv_loader_load_next_frame(struct imv_loader *ldr);

/* Tell the loader that dt time has passed. If the current image is animated,
 * the loader will automatically load the next frame when it is due. */
void imv_loader_time_passed(struct imv_loader *ldr, double dt);

#endif


/* vim:set ts=2 sts=2 sw=2 et: */
