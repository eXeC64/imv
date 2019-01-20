#include "backend_freeimage.h"
#include "backend.h"
#include "source.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <FreeImage.h>

struct private {
  FREE_IMAGE_FORMAT format;
  FIMULTIBITMAP *multibitmap;
  FIBITMAP *last_frame;
  int raw_frame_time;
};

static void time_passed(struct imv_source *src, double dt)
{
  (void)src;
  (void)dt;
}

static double time_left(struct imv_source *src)
{
  (void)src;
  return 0.0;
}

static void source_free(struct imv_source *src)
{
  free(src->name);
  src->name = NULL;

  struct private *private = src->private;

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

  free(src);
}

static struct imv_bitmap *to_imv_bitmap(FIBITMAP *in_bmp)
{
  struct imv_bitmap *bmp = malloc(sizeof(struct imv_bitmap));
  bmp->width = FreeImage_GetWidth(in_bmp);
  bmp->height = FreeImage_GetHeight(in_bmp);
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

  src->callback(&msg);
}

static void send_bitmap(struct imv_source *src, FIBITMAP *fibitmap)
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
  msg.error = NULL;

  src->callback(&msg);
}

static void first_frame(struct imv_source *src)
{
  FIBITMAP *bmp = NULL;

  struct private *private = src->private;

  if (private->format == FIF_GIF) {
    private->multibitmap = FreeImage_OpenMultiBitmap(FIF_GIF, src->name,
        /* don't create file */ 0,
        /* read only */ 1,
        /* keep in memory */ 1,
        /* flags */ GIF_LOAD256);
    if (!private->multibitmap) {
      report_error(src);
      return;
    }

    FIBITMAP *frame = FreeImage_LockPage(private->multibitmap, 0);

    src->num_frames = FreeImage_GetPageCount(private->multibitmap);
    
    /* Get duration of first frame */
    FITAG *tag = NULL;
    FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameTime", &tag);
    if(FreeImage_GetTagValue(tag)) {
      private->raw_frame_time = *(int*)FreeImage_GetTagValue(tag);
    } else {
      private->raw_frame_time = 100; /* default value for gifs */
    }
    bmp = FreeImage_ConvertTo24Bits(frame);
    FreeImage_UnlockPage(private->multibitmap, frame, 0);

  } else { /* not a gif */
    src->num_frames = 1;
    int flags = (private->format == FIF_JPEG) ? JPEG_EXIFROTATE : 0;
    FIBITMAP *fibitmap = FreeImage_Load(private->format, src->name, flags);
    if (!fibitmap) {
      report_error(src);
      return;
    }
    bmp = FreeImage_ConvertTo32Bits(fibitmap);
    FreeImage_Unload(fibitmap);
  }

  src->width = FreeImage_GetWidth(bmp);
  src->height = FreeImage_GetWidth(bmp);
  send_bitmap(src, bmp);
  private->last_frame = bmp;
}

static void next_frame(struct imv_source *src)
{
  struct private *private = src->private;
  if (src->num_frames == 1) {
    send_bitmap(src, private->last_frame);
    return;
  }

  // TODO gifs
}

static enum backend_result open_path(const char *path, struct imv_source **src)
{
  FREE_IMAGE_FORMAT fmt = FreeImage_GetFileType(path, 0);

  if (fmt == FIF_UNKNOWN) {
    return BACKEND_UNSUPPORTED;
  }

  struct private *private = calloc(sizeof(struct private), 1);
  private->format = fmt;

  struct imv_source *source = calloc(sizeof(struct imv_source), 1);
  source->name = strdup(path);
  source->width = 0;
  source->height = 0;
  source->num_frames = 0;
  source->image_event_id = 0;
  source->error_event_id = 0;
  source->load_first_frame = &first_frame;
  source->load_next_frame = &next_frame;
  source->time_passed = &time_passed;
  source->time_left = &time_left;
  source->free = &source_free;
  source->callback = NULL;
  source->user_data = NULL;
  source->private = private;
  *src = source;

  return BACKEND_SUCCESS;
}

static void backend_free(struct imv_backend *backend)
{
  free(backend);
}

struct imv_backend *imv_backend_freeimage(void)
{
  struct imv_backend *backend = malloc(sizeof(struct imv_backend));
  backend->name = "FreeImage (GPL license)";
  backend->open_path = &open_path;
  backend->free = &backend_free;
  return backend;
}
