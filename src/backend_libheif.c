#include <stdlib.h>
#include <string.h>
#include <libheif/heif.h>

#include "backend.h"
#include "bitmap.h"
#include "image.h"
#include "source_private.h"

struct private {
  struct heif_image *img;
};

static void free_private(void *raw_private)
{
  if (!raw_private) {
    return;
  }
  struct private *private = raw_private;
  heif_image_release(private->img);
  free(private);
}

static void load_image(void *raw_private, struct imv_image **image, int *frametime)
{
  *image = NULL;
  *frametime = 0;

  struct private *private = raw_private;

  int stride;
  const uint8_t *data = heif_image_get_plane_readonly(private->img, heif_channel_interleaved, &stride);

  int width = heif_image_get_width(private->img, heif_channel_interleaved);
  int height = heif_image_get_height(private->img, heif_channel_interleaved);
  unsigned char *bitmap = malloc(width * height * 4);
  memcpy(bitmap, data, width * height * 4);

  struct imv_bitmap *bmp = malloc(sizeof *bmp);
  bmp->width = width,
  bmp->height = height,
  bmp->format = IMV_ABGR;
  bmp->data = bitmap;
  *image = imv_image_create_from_bitmap(bmp);
}

static const struct imv_source_vtable vtable = {
  .load_first_frame = load_image,
  .free = free_private,
};

struct heif_error get_primary_image(struct heif_context *ctx, struct heif_image **img)
{
  struct heif_image_handle *handle;
  struct heif_error err = heif_context_get_primary_image_handle(ctx, &handle);
  if (err.code != heif_error_Ok) {
    return err;
  }

  err = heif_decode_image(handle, img, heif_colorspace_RGB, heif_chroma_interleaved_RGBA, NULL);
  heif_image_handle_release(handle);
  return err;
}

static enum backend_result open_path(const char *path, struct imv_source **src)
{
  struct heif_context *ctx = heif_context_alloc();
  struct heif_error err = heif_context_read_from_file(ctx, path, NULL); // TODO: error
  if (err.code != heif_error_Ok) {
    heif_context_free(ctx);
    if (err.code == heif_error_Input_does_not_exist) {
      return BACKEND_BAD_PATH;
    }
    return BACKEND_UNSUPPORTED;
  }

  struct heif_image *img;
  err = get_primary_image(ctx, &img);
  heif_context_free(ctx);
  if (err.code != heif_error_Ok) {
    return BACKEND_UNSUPPORTED;
  }

  struct private *private = malloc(sizeof *private);
  private->img = img;
  *src = imv_source_create(&vtable, private);
  return BACKEND_SUCCESS;
}

static enum backend_result open_memory(void *data, size_t len, struct imv_source **src)
{
  struct heif_context *ctx = heif_context_alloc();
  struct heif_error err = heif_context_read_from_memory_without_copy(ctx, data, len, NULL);
  if (err.code != heif_error_Ok) {
    heif_context_free(ctx);
    return BACKEND_UNSUPPORTED;
  }

  struct heif_image *img;
  err = get_primary_image(ctx, &img);
  heif_context_free(ctx);
  if (err.code != heif_error_Ok) {
    return BACKEND_UNSUPPORTED;
  }

  struct private *private = malloc(sizeof *private);
  private->img = img;
  *src = imv_source_create(&vtable, private);
  return BACKEND_SUCCESS;
}

const struct imv_backend imv_backend_libheif = {
  .name = "libheif",
  .description = "ISO/IEC 23008-12:2017 HEIF file format decoder and encoder.",
  .website = "http://www.libheif.org",
  .license = "GNU Lesser General Public License",
  .open_path = &open_path,
  .open_memory = &open_memory,
};
