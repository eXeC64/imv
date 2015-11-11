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

#include "image.h"

void imv_init_image(struct imv_image *img)
{
  img->mbmp = NULL;
  img->cur_bmp = NULL;
  img->width = 0;
  img->height = 0;
  img->cur_frame = 0;
  img->next_frame = 0;
  img->num_frames = 0;
  img->frame_time = 0;
  img->changed = 0;
}

void imv_destroy_image(struct imv_image *img)
{
  if(img->cur_bmp) {
    FreeImage_Unload(img->cur_bmp);
    img->cur_bmp = NULL;
  }
  if(img->mbmp) {
    FreeImage_CloseMultiBitmap(img->mbmp, 0);
    img->mbmp = NULL;
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

int imv_image_load(struct imv_image *img, const char* path)
{
  if(img->mbmp) {
    FreeImage_CloseMultiBitmap(img->mbmp, 0);
    img->mbmp = NULL;
  }

  if(img->cur_bmp) {
    FreeImage_Unload(img->cur_bmp);
    img->cur_bmp = NULL;
  }


  FREE_IMAGE_FORMAT fmt = FreeImage_GetFileType(path,0);

  if(fmt == FIF_UNKNOWN) {
    return 1;
  }

  img->num_frames = 0;
  img->cur_frame = 0;
  img->next_frame = 0;
  img->frame_time = 0;

  if(fmt == FIF_GIF) {
    img->mbmp = FreeImage_OpenMultiBitmap(FIF_GIF, path,
      /* don't create file */ 0,
      /* read only */ 1,
      /* keep in memory */ 1,
      /* flags */ GIF_LOAD256);
    if(!img->mbmp) {
      return 1;
    }
    img->num_frames = FreeImage_GetPageCount(img->mbmp);

    /* get the dimensions from the first frame */
    FIBITMAP *frame = FreeImage_LockPage(img->mbmp, 0);
    img->width = FreeImage_GetWidth(frame);
    img->height = FreeImage_GetHeight(frame);
    FreeImage_UnlockPage(img->mbmp, frame, 0);

    /* load a frame */
    imv_image_load_next_frame(img);
  } else {
    FIBITMAP *image = FreeImage_Load(fmt, path, 0);
    if(!image) {
      return 1;
    }
    FreeImage_FlipVertical(image);
    img->cur_bmp = FreeImage_ConvertTo32Bits(image);
    img->width = FreeImage_GetWidth(img->cur_bmp);
    img->height = FreeImage_GetHeight(img->cur_bmp);
  }

  img->changed = 1;
  return 0;
}

void imv_image_load_next_frame(struct imv_image *img)
{
  if(!imv_image_is_animated(img)) {
    return;
  }

  FITAG *tag = NULL;
  char disposal_method = 0;
  int frame_time = 0;
  short top = 0;
  short left = 0;

  img->cur_frame = img->next_frame;
  img->next_frame = (img->cur_frame + 1) % img->num_frames;
  FIBITMAP *frame = FreeImage_LockPage(img->mbmp, img->cur_frame);
  FIBITMAP *frame32 = FreeImage_ConvertTo32Bits(frame);
  FreeImage_FlipVertical(frame32);

  /* First frame is always going to use the raw frame */
  if(img->cur_frame > 0) {
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

  img->frame_time += frame_time * 0.001;

  FreeImage_UnlockPage(img->mbmp, frame, 0);

  /* If this frame is inset, we need to expand it for compositing */
  if(left != 0 || top != 0) {
    RGBQUAD color = {0,0,0,0};
    FIBITMAP *expanded = FreeImage_EnlargeCanvas(frame32,
        left,
        img->height - FreeImage_GetHeight(frame32) - top,
        img->width - FreeImage_GetWidth(frame32) - left,
        top,
        &color,
        0);
    FreeImage_Unload(frame32);
    frame32 = expanded;
  }

  switch(disposal_method) {
    case 0: /* nothing specified, just use the raw frame */
      if(img->cur_bmp) {
        FreeImage_Unload(img->cur_bmp);
      }
      img->cur_bmp = frame32;
      break;
    case 1: /* composite over previous frame */
      if(img->cur_bmp && img->cur_frame > 0) {
        FIBITMAP *bg_frame = FreeImage_ConvertTo24Bits(img->cur_bmp);
        FreeImage_Unload(img->cur_bmp);
        FIBITMAP *comp = FreeImage_Composite(frame32, 1, NULL, bg_frame);
        FreeImage_Unload(bg_frame);
        FreeImage_Unload(frame32);
        img->cur_bmp = comp;
      } else {
        /* No previous frame, just render directly */
        if(img->cur_bmp) {
          FreeImage_Unload(img->cur_bmp);
        }
        img->cur_bmp = frame32;
      }
      break;
    case 2: /* TODO - set to background, composite over that */
      break;
    case 3: /* TODO - restore to previous content */
      break;
  }
  img->changed = 1;
}

int imv_image_is_animated(struct imv_image *img)
{
  return img->num_frames > 1;
}

void imv_image_play(struct imv_image *img, double time)
{
  if(!imv_image_is_animated(img)) {
    return;
  }

  img->frame_time -= time;
  if(img->frame_time < 0) {
    img->frame_time = 0;
    imv_image_load_next_frame(img);
  }
}

int imv_image_has_changed(struct imv_image *img)
{
  if(img->changed) {
    img->changed = 0;
    return 1;
  } else {
  return 0;
  }
}
