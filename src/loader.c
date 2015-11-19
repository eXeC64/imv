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

void imv_init_loader(struct imv_loader *ldr)
{
  ldr->mbmp = NULL;
  ldr->cur_bmp = NULL;
  ldr->width = 0;
  ldr->height = 0;
  ldr->cur_frame = 0;
  ldr->next_frame = 0;
  ldr->num_frames = 0;
  ldr->frame_time = 0;
  ldr->changed = 0;
}

void imv_destroy_loader(struct imv_loader *ldr)
{
  if(ldr->cur_bmp) {
    FreeImage_Unload(ldr->cur_bmp);
    ldr->cur_bmp = NULL;
  }
  if(ldr->mbmp) {
    FreeImage_CloseMultiBitmap(ldr->mbmp, 0);
    ldr->mbmp = NULL;
  }
}

int imv_can_load_image(const char* path)
{
  if(FreeImage_GetFileType(path, 0) == FIF_UNKNOWN) {
    return 0;
  } else {
    return 1;
  }
}

int imv_loader_load(struct imv_loader *ldr, const char* path)
{
  if(ldr->mbmp) {
    FreeImage_CloseMultiBitmap(ldr->mbmp, 0);
    ldr->mbmp = NULL;
  }

  if(ldr->cur_bmp) {
    FreeImage_Unload(ldr->cur_bmp);
    ldr->cur_bmp = NULL;
  }


  FREE_IMAGE_FORMAT fmt = FreeImage_GetFileType(path,0);

  if(fmt == FIF_UNKNOWN) {
    return 1;
  }

  ldr->num_frames = 0;
  ldr->cur_frame = 0;
  ldr->next_frame = 0;
  ldr->frame_time = 0;

  if(fmt == FIF_GIF) {
    ldr->mbmp = FreeImage_OpenMultiBitmap(FIF_GIF, path,
      /* don't create file */ 0,
      /* read only */ 1,
      /* keep in memory */ 1,
      /* flags */ GIF_LOAD256);
    if(!ldr->mbmp) {
      return 1;
    }
    ldr->num_frames = FreeImage_GetPageCount(ldr->mbmp);

    /* get the dimensions from the first frame */
    FIBITMAP *frame = FreeImage_LockPage(ldr->mbmp, 0);
    ldr->width = FreeImage_GetWidth(frame);
    ldr->height = FreeImage_GetHeight(frame);
    if(!imv_loader_is_animated(ldr)) {
      ldr->cur_bmp = FreeImage_ConvertTo32Bits(frame);
    }
    FreeImage_UnlockPage(ldr->mbmp, frame, 0);

    if(imv_loader_is_animated(ldr)) {
      /* load a frame */
      imv_loader_load_next_frame(ldr);
    }
  } else {
    FIBITMAP *image = FreeImage_Load(fmt, path, 0);
    if(!image) {
      return 1;
    }
    ldr->cur_bmp = FreeImage_ConvertTo32Bits(image);
    ldr->width = FreeImage_GetWidth(ldr->cur_bmp);
    ldr->height = FreeImage_GetHeight(ldr->cur_bmp);
    FreeImage_Unload(image);
  }

  ldr->changed = 1;
  return 0;
}

void imv_loader_load_next_frame(struct imv_loader *ldr)
{
  if(!imv_loader_is_animated(ldr)) {
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

int imv_loader_is_animated(struct imv_loader *ldr)
{
  return ldr->num_frames > 1;
}

void imv_loader_play(struct imv_loader *ldr, double time)
{
  if(!imv_loader_is_animated(ldr)) {
    return;
  }

  ldr->frame_time -= time;
  if(ldr->frame_time < 0) {
    ldr->frame_time = 0;
    imv_loader_load_next_frame(ldr);
  }
}

int imv_loader_has_changed(struct imv_loader *ldr)
{
  if(ldr->changed) {
    ldr->changed = 0;
    return 1;
  } else {
    return 0;
  }
}
