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

#include "loader.h"
#include "texture.h"
#include <stdlib.h>
#include <pthread.h>

void imv_init_loader(struct imv_loader *ldr)
{
  pthread_mutex_init(&ldr->lock, NULL);
  ldr->bg_thread = 0;
  ldr->path = NULL;
  ldr->out_bmp = NULL;
  ldr->out_err = NULL;
  ldr->mbmp = NULL;
  ldr->bmp = NULL;
  ldr->width = 0;
  ldr->height = 0;
  ldr->cur_frame = 0;
  ldr->next_frame = 0;
  ldr->num_frames = 0;
  ldr->frame_time = 0;
}

void imv_destroy_loader(struct imv_loader *ldr)
{
  /* wait for any existing bg thread to finish */
  pthread_join(ldr->bg_thread, NULL);
  pthread_mutex_destroy(&ldr->lock);

  if(ldr->bmp) {
    FreeImage_Unload(ldr->bmp);
  }
  if(ldr->mbmp) {
    FreeImage_CloseMultiBitmap(ldr->mbmp, 0);
  }
  if(ldr->path) {
    free(ldr->path);
  }
}

static void *imv_loader_bg_new_img(void *data);
static void *imv_loader_bg_next_frame(void *data);

void imv_loader_load_path(struct imv_loader *ldr, const char *path)
{
  pthread_mutex_lock(&ldr->lock);

  /* cancel existing thread if already running */
  if(ldr->bg_thread) {
    /* TODO - change to signal */
    pthread_cancel(ldr->bg_thread);
  }

  /* kick off a new thread to load the image */
  ldr->path = strdup(path);
  pthread_create(&ldr->bg_thread, NULL, &imv_loader_bg_new_img, ldr);

  pthread_mutex_unlock(&ldr->lock);
}

FIBITMAP *imv_loader_get_image(struct imv_loader *ldr)
{
  FIBITMAP *ret = NULL;
  pthread_mutex_lock(&ldr->lock);

  if(ldr->out_bmp) {
    ret = ldr->out_bmp;
    ldr->out_bmp = NULL;
  }

  pthread_mutex_unlock(&ldr->lock);
  return ret;
}

char *imv_loader_get_error(struct imv_loader *ldr)
{
  char *err = NULL;
  pthread_mutex_lock(&ldr->lock);

  if(ldr->out_err) {
    err = ldr->out_err;
    ldr->out_err = NULL;
  }

  pthread_mutex_unlock(&ldr->lock);
  return err;
}

void imv_loader_next_frame(struct imv_loader *ldr)
{
  pthread_mutex_lock(&ldr->lock);
  /* TODO */
  pthread_mutex_unlock(&ldr->lock);
}

void imv_loader_time_passed(struct imv_loader *ldr, double dt)
{
  pthread_mutex_lock(&ldr->lock);
  /* TODO */
  pthread_mutex_unlock(&ldr->lock);
}

void imv_loader_error_occurred(struct imv_loader *ldr)
{
  pthread_mutex_lock(&ldr->lock);
  if(ldr->out_err) {
    free(ldr->out_err);
  }
  ldr->out_err = strdup(ldr->path);
  pthread_mutex_unlock(&ldr->lock);
}

static void *imv_loader_bg_new_img(void *data)
{
  struct imv_loader *ldr = data;

  pthread_mutex_lock(&ldr->lock);
  char *path = strdup(ldr->path);
  pthread_mutex_unlock(&ldr->lock);

  FREE_IMAGE_FORMAT fmt = FreeImage_GetFileType(path, 0);

  if(fmt == FIF_UNKNOWN) {
    imv_loader_error_occurred(ldr);
    free(path);
    return 0;
  }

  int num_frames = 1;
  FIMULTIBITMAP *mbmp = NULL;
  FIBITMAP *bmp = NULL;
  int width, height;

  if(fmt == FIF_GIF) {
    mbmp = FreeImage_OpenMultiBitmap(FIF_GIF, path,
      /* don't create file */ 0,
      /* read only */ 1,
      /* keep in memory */ 1,
      /* flags */ GIF_LOAD256);
    free(path);
    if(!mbmp) {
      imv_loader_error_occurred(ldr);
      return 0;
    }
    num_frames = FreeImage_GetPageCount(mbmp);

    FIBITMAP *frame = FreeImage_LockPage(mbmp, 0);
    width = FreeImage_GetWidth(frame);
    height = FreeImage_GetHeight(frame);
    bmp = FreeImage_ConvertTo32Bits(frame);
    FreeImage_UnlockPage(mbmp, frame, 0);
  } else {
    FIBITMAP *image = FreeImage_Load(fmt, path, 0);
    free(path);
    if(!image) {
      imv_loader_error_occurred(ldr);
      return 0;
    }
    width = FreeImage_GetWidth(bmp);
    height = FreeImage_GetHeight(bmp);
    bmp = FreeImage_ConvertTo32Bits(image);
    FreeImage_Unload(image);
  }

  /* now update the loader */
  pthread_mutex_lock(&ldr->lock);

  if(ldr->mbmp) {
    FreeImage_CloseMultiBitmap(ldr->mbmp, 0);
  }

  if(ldr->bmp) {
    FreeImage_Unload(ldr->bmp);
  }

  ldr->mbmp = mbmp;
  ldr->bmp = bmp;
  ldr->out_bmp = bmp;
  ldr->width = width;
  ldr->height = height;
  ldr->cur_frame = 0;
  ldr->next_frame = 1;
  ldr->num_frames = num_frames;
  ldr->frame_time = 0;

  pthread_mutex_unlock(&ldr->lock);
  return 0;
}

/* TODO - rewrite */
static void *imv_loader_bg_next_frame(void *data)
{
  struct imv_loader *ldr = data;
  return 0;
}

#if 0
  pthread_mutex_lock(&ldr->lock);
  int num_frames = ldr->num_frames;
  pthread_mutex_unlock(&ldr->lock);
  if(num_frames < 2) {
    return;
  }

  FITAG *tag = NULL;
  char disposal_method = 0;
  int frame_time = 0;
  short top = 0;
  short left = 0;

  ldr->cur_frame = ldr->next_frame;
  ldr->next_frame = (ldr->cur_frame + 1) % ldr->num_frames;
  FIBITMAP *frame = FreeImage_LockPage(ldr->mbmp, ldr->cur_frame);
  FIBITMAP *frame32 = FreeImage_ConvertTo32Bits(frame);

  /* First frame is always going to use the raw frame */
  if(ldr->cur_frame > 0) {
    FreeImage_GetMetadata(FIMD_ANIMATION, frame, "DisposalMethod", &tag);
    if(FreeImage_GetTagValue(tag)) {
      disposal_method = *(char*)FreeImage_GetTagValue(tag);
    }
  }

  FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameLeft", &tag);
  if(FreeImage_GetTagValue(tag)) {
    left = *(short*)FreeImage_GetTagValue(tag);
  }

  FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameTop", &tag);
  if(FreeImage_GetTagValue(tag)) {
    top = *(short*)FreeImage_GetTagValue(tag);
  }

  FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameTime", &tag);
  if(FreeImage_GetTagValue(tag)) {
    frame_time = *(int*)FreeImage_GetTagValue(tag);
  }

  /* some gifs don't provide a frame time at all */
  if(frame_time == 0) {
    frame_time = 100;
  }
  ldr->frame_time += frame_time * 0.001;

  FreeImage_UnlockPage(ldr->mbmp, frame, 0);

  /* If this frame is inset, we need to expand it for compositing */
  if(ldr->width != (int)FreeImage_GetWidth(frame32) ||
     ldr->height != (int)FreeImage_GetHeight(frame32)) {
    FIBITMAP *expanded = FreeImage_Allocate(ldr->width, ldr->height, 32, 0,0,0);
    FreeImage_Paste(expanded, frame32, left, top, 255);
    FreeImage_Unload(frame32);
    frame32 = expanded;
  }

  switch(disposal_method) {
    case 0: /* nothing specified, just use the raw frame */
      if(ldr->cur_bmp) {
        FreeImage_Unload(ldr->cur_bmp);
      }
      ldr->cur_bmp = frame32;
      break;
    case 1: /* composite over previous frame */
      if(ldr->cur_bmp && ldr->cur_frame > 0) {
        FIBITMAP *bg_frame = FreeImage_ConvertTo24Bits(ldr->cur_bmp);
        FreeImage_Unload(ldr->cur_bmp);
        FIBITMAP *comp = FreeImage_Composite(frame32, 1, NULL, bg_frame);
        FreeImage_Unload(bg_frame);
        FreeImage_Unload(frame32);
        ldr->cur_bmp = comp;
      } else {
        /* No previous frame, just render directly */
        if(ldr->cur_bmp) {
          FreeImage_Unload(ldr->cur_bmp);
        }
        ldr->cur_bmp = frame32;
      }
      break;
    case 2: /* TODO - set to background, composite over that */
      if(ldr->cur_bmp) {
        FreeImage_Unload(ldr->cur_bmp);
      }
      ldr->cur_bmp = frame32;
      break;
    case 3: /* TODO - restore to previous content */
      if(ldr->cur_bmp) {
        FreeImage_Unload(ldr->cur_bmp);
      }
      ldr->cur_bmp = frame32;
      break;
  }
  ldr->changed = 1;
}
#endif
