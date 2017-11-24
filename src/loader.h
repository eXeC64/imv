#ifndef IMV_LOADER_H
#define IMV_LOADER_H

/* Copyright (c) imv authors

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
  char *out_err;
  FIMULTIBITMAP *mbmp;
  FIBITMAP *bmp;
  int width;
  int height;
  int cur_frame;
  int next_frame;
  int num_frames;
  double frame_time;
  unsigned int new_image_event;
};

/* Creates an instance of imv_loader */
struct imv_loader *imv_loader_create(void);

/* Cleans up an imv_loader instance */
void imv_loader_free(struct imv_loader *ldr);

/* Asynchronously load the given file */
void imv_loader_load(struct imv_loader *ldr, const char *path,
                     const void *buffer, const size_t buffer_size);

/* Set the custom event types for returning data */
void imv_loader_set_event_types(struct imv_loader *ldr, unsigned int new_image);

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

/* Ask the loader how long we can sleep for until the next frame */
double imv_loader_time_left(struct imv_loader *ldr);

#endif


/* vim:set ts=2 sts=2 sw=2 et: */
