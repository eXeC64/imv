#include "backend.h"
#include "source.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <tiffio.h>

struct private {
  TIFF *tiff;
  void *data;
  size_t pos, len;
};

static tsize_t mem_read(thandle_t data, tdata_t buffer, tsize_t len)
{
  struct private *private = (struct private*)data;
  memcpy(buffer, (char*)private->data + private->pos, len);
  private->pos += len;
  return len;
}

static tsize_t mem_write(thandle_t data, tdata_t buffer, tsize_t len)
{
  struct private *private = (struct private*)data;
  memcpy((char*)private->data + private->pos, buffer, len);
  private->pos += len;
  return len;
}

static int mem_close(thandle_t data)
{
  (void)data;
  return 0;
}

static toff_t mem_seek(thandle_t data, toff_t pos, int whence)
{
  struct private *private = (struct private*)data;
  if (whence == SEEK_SET) {
    private->pos = pos;
  } else if (whence == SEEK_CUR) {
    private->pos += pos;
  } else if (whence == SEEK_END) {
    private->pos = private->len + pos;
  } else {
    return -1;
  }
  return private->pos;
}

static toff_t mem_size(thandle_t data)
{
  struct private *private = (struct private*)data;
  return private->len;
}

static void source_free(struct imv_source *src)
{
  pthread_mutex_lock(&src->busy);
  free(src->name);
  src->name = NULL;

  struct private *private = src->private;
  TIFFClose(private->tiff);
  private->tiff = NULL;

  free(src->private);
  src->private = NULL;

  pthread_mutex_unlock(&src->busy);
  pthread_mutex_destroy(&src->busy);
  free(src);
}

static struct imv_image *to_image(int width, int height, void *bitmap)
{
  struct imv_bitmap *bmp = malloc(sizeof *bmp);
  bmp->width = width;
  bmp->height = height;
  bmp->format = IMV_ABGR;
  bmp->data = bitmap;
  struct imv_image *image = imv_image_create_from_bitmap(bmp);
  return image;
}

static void report_error(struct imv_source *src)
{
  assert(src->callback);

  struct imv_source_message msg;
  msg.source = src;
  msg.user_data = src->user_data;
  msg.image = NULL;
  msg.error = "Internal error";

  pthread_mutex_unlock(&src->busy);
  src->callback(&msg);
}

static void send_bitmap(struct imv_source *src, void *bitmap)
{
  assert(src->callback);

  struct imv_source_message msg;
  msg.source = src;
  msg.user_data = src->user_data;
  msg.image = to_image(src->width, src->height, bitmap);
  msg.frametime = 0;
  msg.error = NULL;

  pthread_mutex_unlock(&src->busy);
  src->callback(&msg);
}

static void *load_image(struct imv_source *src)
{
  /* Don't run if this source is already active */
  if (pthread_mutex_trylock(&src->busy)) {
    return NULL;
  }

  struct private *private = src->private;

  /* libtiff suggests using their own allocation routines to support systems
   * with segmented memory. I have no desire to support that, so I'm just
   * going to use vanilla malloc/free. Systems where that isn't acceptable
   * don't have upstream support from imv.
   */
  void *bitmap = malloc(src->height * src->width * 4);
  int rcode = TIFFReadRGBAImageOriented(private->tiff, src->width, src->height,
      bitmap, ORIENTATION_TOPLEFT, 0);

  /* 1 = success, unlike the rest of *nix */
  if (rcode != 1) {
    free(bitmap);
    report_error(src);
    return NULL;
  }

  send_bitmap(src, bitmap);
  return NULL;
}

static enum backend_result open_path(const char *path, struct imv_source **src)
{
  struct private private;

  private.tiff = TIFFOpen(path, "r");
  if (!private.tiff) {
    /* Header is read, so no BAD_PATH check here */
    return BACKEND_UNSUPPORTED;
  }

  unsigned int width, height;
  TIFFGetField(private.tiff, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(private.tiff, TIFFTAG_IMAGELENGTH, &height);

  struct imv_source *source = calloc(1, sizeof *source);
  source->name = strdup(path);
  source->width = width;
  source->height = height;
  source->num_frames = 1;
  source->next_frame = 1;
  pthread_mutex_init(&source->busy, NULL);
  source->load_first_frame = &load_image;
  source->load_next_frame = NULL;
  source->free = &source_free;
  source->callback = NULL;
  source->user_data = NULL;
  source->private = malloc(sizeof private);
  memcpy(source->private, &private, sizeof private);

  *src = source;
  return BACKEND_SUCCESS;
}

static enum backend_result open_memory(void *data, size_t len, struct imv_source **src)
{
  struct private *private = malloc(sizeof *private);
  private->data = data;
  private->len = len;
  private->pos = 0;
  private->tiff = TIFFClientOpen("-", "rm", (thandle_t)private,
      &mem_read, &mem_write, &mem_seek, &mem_close, &mem_size,
      NULL, NULL);
  if (!private->tiff) {
    /* Header is read, so no BAD_PATH check here */
    free(private);
    return BACKEND_UNSUPPORTED;
  }

  unsigned int width, height;
  TIFFGetField(private->tiff, TIFFTAG_IMAGEWIDTH, &width);
  TIFFGetField(private->tiff, TIFFTAG_IMAGELENGTH, &height);

  struct imv_source *source = calloc(1, sizeof *source);
  source->name = strdup("-");
  source->width = width;
  source->height = height;
  source->num_frames = 1;
  source->next_frame = 1;
  pthread_mutex_init(&source->busy, NULL);
  source->load_first_frame = &load_image;
  source->load_next_frame = NULL;
  source->free = &source_free;
  source->callback = NULL;
  source->user_data = NULL;
  source->private = private;

  *src = source;
  return BACKEND_SUCCESS;
}

const struct imv_backend imv_backend_libtiff = {
  .name = "libtiff",
  .description = "The de-facto tiff library",
  .website = "http://www.libtiff.org/",
  .license = "MIT",
  .open_path = &open_path,
  .open_memory = &open_memory,
};
