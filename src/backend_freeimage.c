#include "backend.h"
#include "bitmap.h"
#include "image.h"
#include "log.h"
#include "source.h"
#include "source_private.h"

#include <FreeImage.h>
#include <stdlib.h>
#include <string.h>

struct private {
  char *path;
  FIMEMORY *memory;
  FREE_IMAGE_FORMAT format;
  FIMULTIBITMAP *multibitmap;
  FIBITMAP *last_frame;
  int num_frames;
  int next_frame;
  int width;
  int height;
};

static void free_private(void *raw_private)
{
  if (!raw_private) {
    return;
  }

  struct private *private = raw_private;

  free(private->path);
  private->path = NULL;

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
}

static struct imv_image *to_image(FIBITMAP *in_bmp)
{
  struct imv_bitmap *bmp = malloc(sizeof *bmp);
  bmp->width = FreeImage_GetWidth(in_bmp);
  bmp->height = FreeImage_GetHeight(in_bmp);
  bmp->format = IMV_ARGB;
  bmp->data = malloc(4 * bmp->width * bmp->height);
  FreeImage_ConvertToRawBits(bmp->data, in_bmp, 4 * bmp->width, 32,
      FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK, TRUE);
  struct imv_image *image = imv_image_create_from_bitmap(bmp);
  return image;
}

static FIBITMAP *normalise_bitmap(FIBITMAP *input)
{
  FIBITMAP *output = NULL;

  switch (FreeImage_GetImageType(input)) {
    case FIT_RGB16:
    case FIT_RGBA16:
    case FIT_RGBF:
    case FIT_RGBAF:
      output = FreeImage_ConvertTo32Bits(input);
      FreeImage_Unload(input);
      break;

    case FIT_UINT16:
    case FIT_INT16:
    case FIT_UINT32:
    case FIT_INT32:
    case FIT_FLOAT:
    case FIT_DOUBLE:
    case FIT_COMPLEX:
      output = FreeImage_ConvertTo8Bits(input);
      FreeImage_Unload(input);
      break;

    case FIT_BITMAP:
    default:
      output = input;
  }

  imv_log(IMV_DEBUG,
      "freeimage: bitmap normalised to 32 bits: before=%p after=%p\n",
      input, output);
  return output;
}

static void first_frame(void *raw_private, struct imv_image **image, int *frametime)
{
  *image = NULL;
  *frametime = 0;

  imv_log(IMV_DEBUG, "freeimage: first_frame called\n");

  FIBITMAP *bmp = NULL;

  struct private *private = raw_private;

  if (private->format == FIF_GIF) {
    if (private->path) {
      private->multibitmap = FreeImage_OpenMultiBitmap(FIF_GIF, private->path,
          /* don't create file */ 0,
          /* read only */ 1,
          /* keep in memory */ 1,
          /* flags */ GIF_LOAD256);
    } else if (private->memory) {
      private->multibitmap = FreeImage_LoadMultiBitmapFromMemory(FIF_GIF,
          private->memory,
          /* flags */ GIF_LOAD256);
    } else {
      imv_log(IMV_ERROR, "private->path and private->memory both NULL");
      return;
    }

    if (!private->multibitmap) {
      imv_log(IMV_ERROR, "first frame already loaded");
      return;
    }

    FIBITMAP *frame = FreeImage_LockPage(private->multibitmap, 0);

    private->num_frames = FreeImage_GetPageCount(private->multibitmap);
    
    /* Get duration of first frame */
    FITAG *tag = NULL;
    FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameTime", &tag);
    if (FreeImage_GetTagValue(tag)) {
      *frametime = *(int*)FreeImage_GetTagValue(tag);
    } else {
      *frametime = 100; /* default value for gifs */
    }
    bmp = FreeImage_ConvertTo24Bits(frame);
    FreeImage_UnlockPage(private->multibitmap, frame, 0);

  } else { /* not a gif */
    private->num_frames = 1;
    int flags = (private->format == FIF_JPEG) ? JPEG_EXIFROTATE : 0;
    FIBITMAP *fibitmap = NULL;
    if (private->path) {
      fibitmap = FreeImage_Load(private->format, private->path, flags);
    } else if (private->memory) {
      fibitmap = FreeImage_LoadFromMemory(private->format, private->memory, flags);
    }
    if (!fibitmap) {
      imv_log(IMV_ERROR, "FreeImage_Load returned NULL");
      return;
    }

    bmp = normalise_bitmap(fibitmap);
  }

  private->width = FreeImage_GetWidth(bmp);
  private->height = FreeImage_GetHeight(bmp);
  private->last_frame = bmp;
  private->next_frame = 1 % private->num_frames;

  *image = to_image(bmp);
}

static void next_frame(void *raw_private, struct imv_image **image, int *frametime)
{
  *image = NULL;
  *frametime = 0;

  struct private *private = raw_private;

  if (private->num_frames == 1) {
    *image = to_image(private->last_frame);
    return;
  }

  FITAG *tag = NULL;
  char disposal_method = 0;
  short top = 0;
  short left = 0;

  FIBITMAP *frame = FreeImage_LockPage(private->multibitmap, private->next_frame);
  FIBITMAP *frame32 = FreeImage_ConvertTo32Bits(frame);

  /* First frame is always going to use the raw frame */
  if (private->next_frame > 0) {
    FreeImage_GetMetadata(FIMD_ANIMATION, frame, "DisposalMethod", &tag);
    if (FreeImage_GetTagValue(tag)) {
      disposal_method = *(char*)FreeImage_GetTagValue(tag);
    }
  }

  FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameLeft", &tag);
  if (FreeImage_GetTagValue(tag)) {
    left = *(short*)FreeImage_GetTagValue(tag);
  }

  FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameTop", &tag);
  if (FreeImage_GetTagValue(tag)) {
    top = *(short*)FreeImage_GetTagValue(tag);
  }

  FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameTime", &tag);
  if (FreeImage_GetTagValue(tag)) {
    *frametime = *(int*)FreeImage_GetTagValue(tag);
  }

  /* some gifs don't provide a frame time at all */
  if (*frametime == 0) {
    *frametime = 100;
  }

  FreeImage_UnlockPage(private->multibitmap, frame, 0);

  /* If this frame is inset, we need to expand it for compositing */
  if (private->width != (int)FreeImage_GetWidth(frame32) ||
      private->height != (int)FreeImage_GetHeight(frame32)) {
    FIBITMAP *expanded = FreeImage_Allocate(private->width, private->height, 32, 0,0,0);
    FreeImage_Paste(expanded, frame32, left, top, 255);
    FreeImage_Unload(frame32);
    frame32 = expanded;
  }

  switch(disposal_method) {
    case 0: /* nothing specified, fall through to compositing */
    case 1: /* composite over previous frame */
      if (private->last_frame && private->next_frame > 0) {
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
      if (private->last_frame) {
        FreeImage_Unload(private->last_frame);
      }
      private->last_frame = frame32;
      break;
    case 3: /* TODO - restore to previous content */
      if (private->last_frame) {
        FreeImage_Unload(private->last_frame);
      }
      private->last_frame = frame32;
      break;
  }

  private->next_frame = (private->next_frame + 1) % private->num_frames;

  *image = to_image(private->last_frame);
}

static const struct imv_source_vtable vtable = {
  .load_first_frame = first_frame,
  .load_next_frame = next_frame,
  .free = free_private
};

static enum backend_result open_path(const char *path, struct imv_source **src)
{
  imv_log(IMV_DEBUG, "freeimage: open_path(%s)\n", path);
  FREE_IMAGE_FORMAT fmt = FreeImage_GetFileType(path, 0);

  if (fmt == FIF_UNKNOWN) {
    imv_log(IMV_DEBUG, "freeimage: unknown file format\n");
    return BACKEND_UNSUPPORTED;
  }

  struct private *private = calloc(1, sizeof(struct private));
  private->format = fmt;
  private->path = strdup(path);

  *src = imv_source_create(&vtable, private);
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
  private->path = NULL;

  *src = imv_source_create(&vtable, private);
  return BACKEND_SUCCESS;
}

const struct imv_backend imv_backend_freeimage = {
  .name = "FreeImage",
  .description = "Open source image library supporting a large number of formats",
  .website = "http://freeimage.sourceforge.net/",
  .license = "FreeImage Public License v1.0",
  .open_path = &open_path,
  .open_memory = &open_memory,
};
