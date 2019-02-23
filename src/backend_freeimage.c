#include "backend_freeimage.h"
#include "backend.h"
#include "source.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <FreeImage.h>

struct private {
  FIMEMORY *memory;
  FREE_IMAGE_FORMAT format;
  FIMULTIBITMAP *multibitmap;
  FIBITMAP *last_frame;
};

static void source_free(struct imv_source *src)
{
  pthread_mutex_lock(&src->busy);
  free(src->name);
  src->name = NULL;

  struct private *private = src->private;

  if (private->memory) {
    FreeImage_CloseMemory(private->memory);
    private->memory = NULL;
  }

  if (private->multibitmap) {
    FreeImage_CloseMultiBitmap(private->multibitmap, 0);
    private->multibitmap = NULL;
  }

  if (private->last_frame) {
    FreeImage_Unload(private->last_frame);
    private->last_frame = NULL;
  }

  free(private);
  src->private = NULL;

  pthread_mutex_unlock(&src->busy);
  pthread_mutex_destroy(&src->busy);
  free(src);
}

static struct imv_bitmap *to_imv_bitmap(FIBITMAP *in_bmp)
{
  struct imv_bitmap *bmp = malloc(sizeof *bmp);
  bmp->width = FreeImage_GetWidth(in_bmp);
  bmp->height = FreeImage_GetHeight(in_bmp);
  bmp->format = IMV_ARGB;
  bmp->data = malloc(4 * bmp->width * bmp->height);
  FreeImage_ConvertToRawBits(bmp->data, in_bmp, 4 * bmp->width, 32,
      FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK, TRUE);
  return bmp;
}

static void report_error(struct imv_source *src)
{
  if (!src->callback) {
    fprintf(stderr, "imv_source(%s) has no callback configured. "
                    "Discarding error.\n", src->name);
    return;
  }

  struct imv_source_message msg;
  msg.source = src;
  msg.user_data = src->user_data;
  msg.bitmap = NULL;
  msg.error = "Internal error";

  pthread_mutex_unlock(&src->busy);
  src->callback(&msg);
}

static void send_bitmap(struct imv_source *src, FIBITMAP *fibitmap, int frametime)
{
  if (!src->callback) {
    fprintf(stderr, "imv_source(%s) has no callback configured. "
                    "Discarding result.\n", src->name);
    return;
  }

  struct imv_source_message msg;
  msg.source = src;
  msg.user_data = src->user_data;
  msg.bitmap = to_imv_bitmap(fibitmap);
  msg.frametime = frametime;
  msg.error = NULL;

  pthread_mutex_unlock(&src->busy);
  src->callback(&msg);
}

static int first_frame(struct imv_source *src)
{
  /* Don't run if this source is already active */
  if (pthread_mutex_trylock(&src->busy)) {
    return -1;
  }

  FIBITMAP *bmp = NULL;

  struct private *private = src->private;

  int frametime = 0;

  if (private->format == FIF_GIF) {
    if (src->name) {
      private->multibitmap = FreeImage_OpenMultiBitmap(FIF_GIF, src->name,
          /* don't create file */ 0,
          /* read only */ 1,
          /* keep in memory */ 1,
          /* flags */ GIF_LOAD256);
    } else if (private->memory) {
      private->multibitmap = FreeImage_LoadMultiBitmapFromMemory(FIF_GIF,
          private->memory,
          /* flags */ GIF_LOAD256);
    } else {
      report_error(src);
      return -1;
    }

    if (!private->multibitmap) {
      report_error(src);
      return -1;
    }

    FIBITMAP *frame = FreeImage_LockPage(private->multibitmap, 0);

    src->num_frames = FreeImage_GetPageCount(private->multibitmap);
    
    /* Get duration of first frame */
    FITAG *tag = NULL;
    FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameTime", &tag);
    if(FreeImage_GetTagValue(tag)) {
      frametime = *(int*)FreeImage_GetTagValue(tag);
    } else {
      frametime = 100; /* default value for gifs */
    }
    bmp = FreeImage_ConvertTo24Bits(frame);
    FreeImage_UnlockPage(private->multibitmap, frame, 0);

  } else { /* not a gif */
    src->num_frames = 1;
    int flags = (private->format == FIF_JPEG) ? JPEG_EXIFROTATE : 0;
    FIBITMAP *fibitmap = NULL;
    if (src->name) {
      fibitmap = FreeImage_Load(private->format, src->name, flags);
    } else if (private->memory) {
      fibitmap = FreeImage_LoadFromMemory(private->format, private->memory, flags);
    }
    if (!fibitmap) {
      report_error(src);
      return -1;
    }
    bmp = FreeImage_ConvertTo32Bits(fibitmap);
    FreeImage_Unload(fibitmap);
  }

  src->width = FreeImage_GetWidth(bmp);
  src->height = FreeImage_GetHeight(bmp);
  private->last_frame = bmp;
  send_bitmap(src, bmp, frametime);
  return 0;
}

static int next_frame(struct imv_source *src)
{
  /* Don't run if this source is already active */
  if (pthread_mutex_trylock(&src->busy)) {
    return -1;
  }

  struct private *private = src->private;
  if (src->num_frames == 1) {
    send_bitmap(src, private->last_frame, 0);
    return 0;
  }

  FITAG *tag = NULL;
  char disposal_method = 0;
  int frametime = 0;
  short top = 0;
  short left = 0;

  FIBITMAP *frame = FreeImage_LockPage(private->multibitmap, src->next_frame);
  FIBITMAP *frame32 = FreeImage_ConvertTo32Bits(frame);

  /* First frame is always going to use the raw frame */
  if(src->next_frame > 0) {
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
    frametime = *(int*)FreeImage_GetTagValue(tag);
  }

  /* some gifs don't provide a frame time at all */
  if(frametime == 0) {
    frametime = 100;
  }

  FreeImage_UnlockPage(private->multibitmap, frame, 0);

  /* If this frame is inset, we need to expand it for compositing */
  if(src->width != (int)FreeImage_GetWidth(frame32) ||
     src->height != (int)FreeImage_GetHeight(frame32)) {
    FIBITMAP *expanded = FreeImage_Allocate(src->width, src->height, 32, 0,0,0);
    FreeImage_Paste(expanded, frame32, left, top, 255);
    FreeImage_Unload(frame32);
    frame32 = expanded;
  }

  switch(disposal_method) {
    case 0: /* nothing specified, fall through to compositing */
    case 1: /* composite over previous frame */
      if(private->last_frame && src->next_frame > 0) {
        FIBITMAP *bg_frame = FreeImage_ConvertTo24Bits(private->last_frame);
        FreeImage_Unload(private->last_frame);
        FIBITMAP *comp = FreeImage_Composite(frame32, 1, NULL, bg_frame);
        FreeImage_Unload(bg_frame);
        FreeImage_Unload(frame32);
        private->last_frame = comp;
      } else {
        /* No previous frame, just render directly */
        if(private->last_frame) {
          FreeImage_Unload(private->last_frame);
        }
        private->last_frame = frame32;
      }
      break;
    case 2: /* TODO - set to background, composite over that */
      if(private->last_frame) {
        FreeImage_Unload(private->last_frame);
      }
      private->last_frame = frame32;
      break;
    case 3: /* TODO - restore to previous content */
      if(private->last_frame) {
        FreeImage_Unload(private->last_frame);
      }
      private->last_frame = frame32;
      break;
  }

  src->next_frame = (src->next_frame + 1) % src->num_frames;

  send_bitmap(src, private->last_frame, frametime);
  return 0;
}

static enum backend_result open_path(const char *path, struct imv_source **src)
{
  FREE_IMAGE_FORMAT fmt = FreeImage_GetFileType(path, 0);

  if (fmt == FIF_UNKNOWN) {
    return BACKEND_UNSUPPORTED;
  }

  struct private *private = calloc(1, sizeof(struct private));
  private->format = fmt;

  struct imv_source *source = calloc(1, sizeof *source);
  source->name = strdup(path);
  source->width = 0;
  source->height = 0;
  source->num_frames = 0;
  pthread_mutex_init(&source->busy, NULL);
  source->load_first_frame = &first_frame;
  source->load_next_frame = &next_frame;
  source->free = &source_free;
  source->callback = NULL;
  source->user_data = NULL;
  source->private = private;
  *src = source;

  return BACKEND_SUCCESS;
}

static enum backend_result open_memory(void *data, size_t len, struct imv_source **src)
{
  FIMEMORY *fmem = FreeImage_OpenMemory(data, len);

  FREE_IMAGE_FORMAT fmt = FreeImage_GetFileTypeFromMemory(fmem, 0);

  if (fmt == FIF_UNKNOWN) {
    FreeImage_CloseMemory(fmem);
    return BACKEND_UNSUPPORTED;
  }

  struct private *private = calloc(1, sizeof(struct private));
  private->format = fmt;
  private->memory = fmem;

  struct imv_source *source = calloc(1, sizeof *source);
  source->name = NULL;
  source->width = 0;
  source->height = 0;
  source->num_frames = 0;
  pthread_mutex_init(&source->busy, NULL);
  source->load_first_frame = &first_frame;
  source->load_next_frame = &next_frame;
  source->free = &source_free;
  source->callback = NULL;
  source->user_data = NULL;
  source->private = private;
  *src = source;

  return BACKEND_SUCCESS;
}

const struct imv_backend freeimage_backend = {
  .name = "FreeImage",
  .description = "Open source image library supporting a large number of formats",
  .website = "http://freeimage.sourceforge.net/",
  .license = "FreeImage Public License v1.0",
  .open_path = &open_path,
  .open_memory = &open_memory,
};

const struct imv_backend *imv_backend_freeimage(void)
{
  return &freeimage_backend;
}
