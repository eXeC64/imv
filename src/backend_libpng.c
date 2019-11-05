#include "backend.h"
#include "bitmap.h"
#include "image.h"
#include "log.h"
#include "source.h"
#include "source_private.h"

#include <stdlib.h>

#include <png.h>

struct private {
  FILE *file;
  png_structp png;
  png_infop info;
};

static void free_private(void *raw_private)
{
  if (!raw_private) {
    return;
  }

  struct private *private = raw_private;
  png_destroy_read_struct(&private->png, &private->info, NULL);
  if (private->file) {
    fclose(private->file);
  }
  free(private);
}

static void load_image(void *raw_private, struct imv_image **image, int *frametime)
{
  *image = NULL;
  *frametime = 0;

  struct private *private = raw_private;
  if (setjmp(png_jmpbuf(private->png))) {
    return;
  }

  const int width = png_get_image_width(private->png, private->info);
  const int height = png_get_image_height(private->png, private->info);

  png_bytep *rows = malloc(sizeof(png_bytep) * height);
  size_t row_len = png_get_rowbytes(private->png, private->info);
  rows[0] = malloc(height * row_len);
  for (int y = 1; y < height; ++y) {
    rows[y] = rows[0] + row_len * y;
  }

  if (setjmp(png_jmpbuf(private->png))) {
    return;
  }

  png_read_image(private->png, rows);
  void *raw_bmp = rows[0];
  free(rows);
  fclose(private->file);
  private->file = NULL;


  struct imv_bitmap *bmp = malloc(sizeof *bmp);
  bmp->width = width;
  bmp->height = height;
  bmp->format = IMV_ABGR;
  bmp->data = raw_bmp;
  *image = imv_image_create_from_bitmap(bmp);
}

static const struct imv_source_vtable vtable = {
  .load_first_frame = load_image,
  .free = free_private
};

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

  struct private *private = calloc(1, sizeof *private);
  private->file = f;
  private->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!private->png) {
    fclose(private->file);
    free(private);
    return BACKEND_UNSUPPORTED;
  }

  /* set max PNG chunk size to 50MB, instead of 8MB default */
  png_set_chunk_malloc_max(private->png, 1024 * 1024 * 50);

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
  png_set_strip_16(private->png);
  png_set_expand(private->png);
  png_set_packing(private->png);
  png_read_update_info(private->png, private->info);
  imv_log(IMV_DEBUG, "libpng: info width=%d height=%d bit_depth=%d color_type=%d\n",
      png_get_image_width(private->png, private->info),
      png_get_image_height(private->png, private->info),
      png_get_bit_depth(private->png, private->info),
      png_get_color_type(private->png, private->info));

  if (setjmp(png_jmpbuf(private->png))) {
    png_destroy_read_struct(&private->png, &private->info, NULL);
    fclose(private->file);
    free(private);
    return BACKEND_UNSUPPORTED;
  }

  *src = imv_source_create(&vtable, private);
  return BACKEND_SUCCESS;
}

const struct imv_backend imv_backend_libpng = {
  .name = "libpng",
  .description = "The official PNG reference implementation",
  .website = "http://www.libpng.org/pub/png/libpng.html",
  .license = "The libpng license",
  .open_path = &open_path,
};

