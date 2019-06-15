#include "backend_libpng.h"
#include "backend.h"
#include "source.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <png.h>

struct private {
  FILE *file;
  png_structp png;
  png_infop info;
};

static void source_free(struct imv_source *src)
{
  pthread_mutex_lock(&src->busy);
  free(src->name);
  src->name = NULL;

  struct private *private = src->private;
  png_destroy_read_struct(&private->png, &private->info, NULL);
  if (private->file) {
    fclose(private->file);
  }

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

  if (setjmp(png_jmpbuf(private->png))) {
    report_error(src);
    return NULL;
  }

  png_bytep *rows = malloc(sizeof(png_bytep) * src->height);
  size_t row_len = png_get_rowbytes(private->png, private->info);
  rows[0] = malloc(src->height * row_len);
  for (int y = 1; y < src->height; ++y) {
    rows[y] = rows[0] + row_len * y;
  }

  if (setjmp(png_jmpbuf(private->png))) {
    free(rows[0]);
    free(rows);
    report_error(src);
    return NULL;
  }

  png_read_image(private->png, rows);
  void *bmp = rows[0];
  free(rows);
  fclose(private->file);
  private->file = NULL;
  send_bitmap(src, bmp);
  return NULL;
}

static enum backend_result open_path(const char *path, struct imv_source **src)
{

  unsigned char header[8];
  FILE *f = fopen(path, "rb");
  if (!f) {
    return BACKEND_BAD_PATH;
  }
  fread(header, 1, sizeof header, f);
  if (png_sig_cmp(header, 0, sizeof header)) {
    fclose(f);
    return BACKEND_UNSUPPORTED;
  }

  struct private *private = malloc(sizeof *private);
  private->file = f;
  private->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!private->png) {
    fclose(private->file);
    free(private);
    return BACKEND_UNSUPPORTED;
  }

  private->info = png_create_info_struct(private->png);
  if (!private->info) {
    png_destroy_read_struct(&private->png, NULL, NULL);
    fclose(private->file);
    free(private);
    return BACKEND_UNSUPPORTED;
  }

  if (setjmp(png_jmpbuf(private->png))) {
    png_destroy_read_struct(&private->png, &private->info, NULL);
    fclose(private->file);
    free(private);
    return BACKEND_UNSUPPORTED;
  }

  png_init_io(private->png, private->file);
  png_set_sig_bytes(private->png, sizeof header);
  png_read_info(private->png, private->info);

  /* Tell libpng to give us a consistent output format */
  png_set_gray_to_rgb(private->png);
  png_set_filler(private->png, 0xff, PNG_FILLER_AFTER);
  png_read_update_info(private->png, private->info);

  struct imv_source *source = calloc(1, sizeof *source);
  source->name = strdup(path);
  if (setjmp(png_jmpbuf(private->png))) {
    free(source->name);
    free(source);
    png_destroy_read_struct(&private->png, &private->info, NULL);
    fclose(private->file);
    free(private);
    return BACKEND_UNSUPPORTED;
  }

  source->width = png_get_image_width(private->png, private->info);
  source->height = png_get_image_height(private->png, private->info);
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

const struct imv_backend libpng_backend = {
  .name = "libpng",
  .description = "The official PNG reference implementation",
  .website = "http://www.libpng.org/pub/png/libpng.html",
  .license = "The libpng license",
  .open_path = &open_path,
};

const struct imv_backend *imv_backend_libpng(void)
{
  return &libpng_backend;
}
