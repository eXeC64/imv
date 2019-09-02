#include "backend.h"
#include "bitmap.h"
#include "image.h"
#include "log.h"
#include "source.h"
#include "source_private.h"

#include <fcntl.h>
#include <libnsgif.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

struct private {
  int current_frame;
  gif_animation gif;
  void *data;
  size_t len;
};

static void* bitmap_create(int width, int height)
{
  const size_t bytes_per_pixel = 4;
  return calloc(width * height, bytes_per_pixel);
}

static void bitmap_destroy(void *bitmap)
{
  free(bitmap);
}

static unsigned char* bitmap_get_buffer(void *bitmap)
{
  return bitmap;
}

static void bitmap_set_opaque(void *bitmap, bool opaque)
{
  (void)bitmap;
  (void)opaque;
}

static bool bitmap_test_opaque(void *bitmap)
{
  (void)bitmap;
  return false;
}

static void bitmap_mark_modified(void *bitmap)
{
  (void)bitmap;
}

static gif_bitmap_callback_vt bitmap_callbacks = {
  bitmap_create,
  bitmap_destroy,
  bitmap_get_buffer,
  bitmap_set_opaque,
  bitmap_test_opaque,
  bitmap_mark_modified
};


static void free_private(void *raw_private)
{
  if (!raw_private) {
    return;
  }

  struct private *private = raw_private;
  gif_finalise(&private->gif);
  munmap(private->data, private->len);
  free(private);
}

static void push_current_image(struct private *private,
    struct imv_image **image, int *frametime)
{
  struct imv_bitmap *bmp = malloc(sizeof *bmp);
  bmp->width = private->gif.width;
  bmp->height = private->gif.height;
  bmp->format = IMV_ABGR;
  size_t len = 4 * bmp->width * bmp->height;
  bmp->data = malloc(len);
  memcpy(bmp->data, private->gif.frame_image, len);

  *image = imv_image_create_from_bitmap(bmp);
  *frametime = private->gif.frames[private->current_frame].frame_delay * 10.0;
}

static void first_frame(void *raw_private, struct imv_image **image, int *frametime)
{
  *image = NULL;
  *frametime = 0;

  struct private *private = raw_private;
  private->current_frame = 0;

  gif_result code = gif_decode_frame(&private->gif, private->current_frame);
  if (code != GIF_OK) {
    imv_log(IMV_DEBUG, "libnsgif: failed to decode first frame\n");
    return;
  }

  push_current_image(private, image, frametime);
}

static void next_frame(void *raw_private, struct imv_image **image, int *frametime)
{
  *image = NULL;
  *frametime = 0;

  struct private *private = raw_private;

  private->current_frame++;
  private->current_frame %= private->gif.frame_count;

  gif_result code = gif_decode_frame(&private->gif, private->current_frame);
  if (code != GIF_OK) {
    imv_log(IMV_DEBUG, "libnsgif: failed to decode a frame\n");
    return;
  }

  push_current_image(private, image, frametime);
}

static const struct imv_source_vtable vtable = {
  .load_first_frame = first_frame,
  .load_next_frame = next_frame,
  .free = free_private
};

static enum backend_result open_memory(void *data, size_t len, struct imv_source **src)
{
  struct private *private = calloc(1, sizeof *private);
  gif_create(&private->gif, &bitmap_callbacks);

  gif_result code;
  do {
    code = gif_initialise(&private->gif, len, data);
  } while (code == GIF_WORKING);

  if (code != GIF_OK) {
    gif_finalise(&private->gif);
    free(private);
    imv_log(IMV_DEBUG, "libsngif: unsupported file\n");
    return BACKEND_UNSUPPORTED;
  }

  *src = imv_source_create(&vtable, private);
  return BACKEND_SUCCESS;
}

static enum backend_result open_path(const char *path, struct imv_source **src)
{
  imv_log(IMV_DEBUG, "libnsgif: open_path(%s)\n", path);

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    return BACKEND_BAD_PATH;
  }

  off_t len = lseek(fd, 0, SEEK_END);
  if (len < 0) {
    close(fd);
    return BACKEND_BAD_PATH;
  }

  void *data = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (data == MAP_FAILED || !data) {
    return BACKEND_BAD_PATH;
  }

  struct private *private = calloc(1, sizeof *private);
  private->data = data;
  private->len = len;
  gif_create(&private->gif, &bitmap_callbacks);

  gif_result code;
  do {
    code = gif_initialise(&private->gif, private->len, private->data);
  } while (code == GIF_WORKING);

  if (code != GIF_OK) {
    gif_finalise(&private->gif);
    munmap(private->data, private->len);
    free(private);
    imv_log(IMV_DEBUG, "libsngif: unsupported file\n");
    return BACKEND_UNSUPPORTED;
  }

  imv_log(IMV_DEBUG, "libnsgif: num_frames=%d\n", private->gif.frame_count);
  imv_log(IMV_DEBUG, "libnsgif: width=%d\n", private->gif.width);
  imv_log(IMV_DEBUG, "libnsgif: height=%d\n", private->gif.height);

  *src = imv_source_create(&vtable, private);
  return BACKEND_SUCCESS;
}


const struct imv_backend imv_backend_libnsgif = {
  .name = "libnsgif",
  .description = "Tiny GIF decoding library from the NetSurf project",
  .website = "https://www.netsurf-browser.org/projects/libnsgif/",
  .license = "MIT",
  .open_path = &open_path,
  .open_memory = &open_memory,
};
