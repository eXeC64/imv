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
#include <signal.h>

static void block_usr1_signal()
{
  sigset_t sigmask;
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGUSR1);
  sigprocmask(SIG_SETMASK, &sigmask, NULL);
}

static int is_thread_cancelled()
{
  sigset_t sigmask;
  sigpending(&sigmask);
  return sigismember(&sigmask, SIGUSR1);
}

void imv_init_loader(struct imv_loader *ldr, char* stdin_buffer, size_t stdin_buffer_size)
{
  memset(ldr, 0, sizeof(struct imv_loader));
  ldr->stdin_buffer = stdin_buffer;
  ldr->stdin_buffer_size = stdin_buffer_size;
  pthread_mutex_init(&ldr->lock, NULL);
  /* ignore this signal in case we accidentally receive it */
  block_usr1_signal();
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
  /* cancel existing thread if already running */
  if(ldr->bg_thread) {
    pthread_kill(ldr->bg_thread, SIGUSR1);
  }

  /* kick off a new thread to load the image */
  pthread_mutex_lock(&ldr->lock);
  if(ldr->path) {
    free(ldr->path);
  }
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

void imv_loader_load_next_frame(struct imv_loader *ldr)
{
  /* wait for existing thread to finish if already running */
  if(ldr->bg_thread) {
    pthread_join(ldr->bg_thread, NULL);
  }

  /* kick off a new thread */
  pthread_create(&ldr->bg_thread, NULL, &imv_loader_bg_next_frame, ldr);
}

void imv_loader_time_passed(struct imv_loader *ldr, double dt)
{
  int get_frame = 0;
  pthread_mutex_lock(&ldr->lock);
  if(ldr->num_frames > 1) {
    ldr->frame_time -= dt;
    if(ldr->frame_time < 0) {
      get_frame = 1;
    }
  }
  pthread_mutex_unlock(&ldr->lock);

  if(get_frame) {
    imv_loader_load_next_frame(ldr);
  }
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
  /* so we can poll for it */
  block_usr1_signal();

  struct imv_loader *ldr = data;

  pthread_mutex_lock(&ldr->lock);
  char *path = strdup(ldr->path);
  pthread_mutex_unlock(&ldr->lock);
  FREE_IMAGE_FORMAT fmt = NULL;
  FIBITMAP *bmp = NULL;

  int load_from_memory = 0;
  if(ldr->stdin_buffer != NULL && strcmp(path, "stdin") == 0){
    load_from_memory = 1;
  }

  if(load_from_memory == 1){
  FIMEMORY *imgdata = FreeImage_OpenMemory((BYTE*)ldr->stdin_buffer, 
          ldr->stdin_buffer_size);
    fmt = FreeImage_GetFileTypeFromMemory(imgdata, 0);
    if(fmt != FIF_UNKNOWN) {
      /* load from the file memory */
      bmp = FreeImage_LoadFromMemory(fmt, imgdata, 0);
    } else {
      imv_loader_error_occurred(ldr);
      return 0;
    }
  }
  else
  {
      fmt = FreeImage_GetFileType(path, 0);
  }

  if(fmt == FIF_UNKNOWN) {
    imv_loader_error_occurred(ldr);
    free(path);
    return 0;
  }

  int num_frames = 1;
  FIMULTIBITMAP *mbmp = NULL;
  int width, height;
  int raw_frame_time = 100; /* default to 100 */

  if(fmt == FIF_GIF) {
    if(load_from_memory == 1){
      imv_loader_error_occurred(ldr);
      fprintf(stderr, "Loading of gifs via stdin is not supported.\n");
      return 0;
    }

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

    /* get duration of first frame */
    FITAG *tag = NULL;
    FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameTime", &tag);
    if(FreeImage_GetTagValue(tag)) {
      raw_frame_time = *(int*)FreeImage_GetTagValue(tag);
    }
    FreeImage_UnlockPage(mbmp, frame, 0);
  } else {
    /* Future TODO: If we load image line-by-line we could stop loading large
     * ones before wasting much more time/memory on them. */
    FIBITMAP *image = NULL;
    if(load_from_memory != 1){
      image = FreeImage_Load(fmt, path, 0);
      free(path);
      if(!image) {
        imv_loader_error_occurred(ldr);
        return 0;
      }
    } else {
      image = bmp;
    }
    /* Check for cancellation before we convert pixel format */
    if(is_thread_cancelled()) {
      FreeImage_Unload(image);
      return 0;
    }

    width = FreeImage_GetWidth(bmp);
    height = FreeImage_GetHeight(bmp);
    bmp = FreeImage_ConvertTo32Bits(image);
    FreeImage_Unload(image);
  }

  /* now update the loader */
  pthread_mutex_lock(&ldr->lock);

  /* check for cancellation before finishing */
  if(is_thread_cancelled()) {
    if(mbmp) {
      FreeImage_CloseMultiBitmap(mbmp, 0);
    }
    if(bmp) {
      FreeImage_Unload(bmp);
    }
    pthread_mutex_unlock(&ldr->lock);
    return 0;
  }

  if(ldr->mbmp) {
    FreeImage_CloseMultiBitmap(ldr->mbmp, 0);
  }

  if(ldr->bmp) {
    FreeImage_Unload(ldr->bmp);
  }

  ldr->mbmp = mbmp;
  ldr->bmp = bmp;
  if(ldr->out_bmp) {
    FreeImage_Unload(ldr->out_bmp);
  }
  ldr->out_bmp = FreeImage_Clone(bmp);
  ldr->width = width;
  ldr->height = height;
  ldr->cur_frame = 0;
  ldr->next_frame = 1;
  ldr->num_frames = num_frames;
  ldr->frame_time = (double)raw_frame_time * 0.0001;

  pthread_mutex_unlock(&ldr->lock);
  return 0;
}

static void *imv_loader_bg_next_frame(void *data)
{
  struct imv_loader *ldr = data;

  pthread_mutex_lock(&ldr->lock);
  int num_frames = ldr->num_frames;
  if(num_frames < 2) {
    pthread_mutex_unlock(&ldr->lock);
    return 0;
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
      if(ldr->bmp) {
        FreeImage_Unload(ldr->bmp);
      }
      ldr->bmp = frame32;
      break;
    case 1: /* composite over previous frame */
      if(ldr->bmp && ldr->cur_frame > 0) {
        FIBITMAP *bg_frame = FreeImage_ConvertTo24Bits(ldr->bmp);
        FreeImage_Unload(ldr->bmp);
        FIBITMAP *comp = FreeImage_Composite(frame32, 1, NULL, bg_frame);
        FreeImage_Unload(bg_frame);
        FreeImage_Unload(frame32);
        ldr->bmp = comp;
      } else {
        /* No previous frame, just render directly */
        if(ldr->bmp) {
          FreeImage_Unload(ldr->bmp);
        }
        ldr->bmp = frame32;
      }
      break;
    case 2: /* TODO - set to background, composite over that */
      if(ldr->bmp) {
        FreeImage_Unload(ldr->bmp);
      }
      ldr->bmp = frame32;
      break;
    case 3: /* TODO - restore to previous content */
      if(ldr->bmp) {
        FreeImage_Unload(ldr->bmp);
      }
      ldr->bmp = frame32;
      break;
  }

  if(ldr->out_bmp) {
    FreeImage_Unload(ldr->out_bmp);
  }
  ldr->out_bmp = FreeImage_Clone(ldr->bmp);

  pthread_mutex_unlock(&ldr->lock);
  return 0;
}
