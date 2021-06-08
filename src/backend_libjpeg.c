#include "backend.h"
#include "bitmap.h"
#include "image.h"
#include "source.h"
#include "source_private.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <turbojpeg.h>

struct private {
  int fd;
  void *data;
  size_t len;
  tjhandle jpeg;
  int width;
  int height;
};

static void free_private(void *raw_private)
{
  if (!raw_private) {
    return;
  }
  struct private *private = raw_private;
  tjDestroy(private->jpeg);
  if (private->fd >= 0) {
    munmap(private->data, private->len);
    close(private->fd);
  }

  free(private);
}

static void load_image(void *raw_private, struct imv_image **image, int *frametime)
{
  *image = NULL;
  *frametime = 0;

  struct private *private = raw_private;

  void *bitmap = malloc(private->height * private->width * 4);
  int rcode = tjDecompress2(private->jpeg, private->data, private->len,
      bitmap, private->width, 0, private->height, TJPF_RGBA, TJFLAG_FASTDCT);

  if (rcode) {
    free(bitmap);
    return;
  }

  struct imv_bitmap *bmp = malloc(sizeof *bmp);
  bmp->width = private->width;
  bmp->height = private->height;
  bmp->format = IMV_ABGR;
  bmp->data = bitmap;
  *image = imv_image_create_from_bitmap(bmp);
}

static const struct imv_source_vtable vtable = {
  .load_first_frame = load_image,
  .free = free_private
};

static enum backend_result open_path(const char *path, struct imv_source **src)
{
  struct private private;

  private.fd = open(path, O_RDONLY);
  if (private.fd < 0) {
    return BACKEND_BAD_PATH;
  }

  off_t len = lseek(private.fd, 0, SEEK_END);
  if (len < 0) {
    close(private.fd);
    return BACKEND_BAD_PATH;
  }

  private.len = len;

  private.data = mmap(NULL, private.len, PROT_READ, MAP_PRIVATE, private.fd, 0);
  if (private.data == MAP_FAILED || !private.data) {
    close(private.fd);
    return BACKEND_BAD_PATH;
  }

  private.jpeg = tjInitDecompress();
  if (!private.jpeg) {
    munmap(private.data, private.len);
    close(private.fd);
    return BACKEND_UNSUPPORTED;
  }

  int rcode = tjDecompressHeader(private.jpeg, private.data, private.len,
      &private.width, &private.height);
  if (rcode) {
    tjDestroy(private.jpeg);
    munmap(private.data, private.len);
    close(private.fd);
    return BACKEND_UNSUPPORTED;
  }

  struct private *new_private = malloc(sizeof private);
  memcpy(new_private, &private, sizeof private);

  *src = imv_source_create(&vtable, new_private);
  return BACKEND_SUCCESS;
}

static enum backend_result open_memory(void *data, size_t len, struct imv_source **src)
{
  struct private private;

  private.fd = -1;
  private.data = data;
  private.len = len;

  private.jpeg = tjInitDecompress();
  if (!private.jpeg) {
    return BACKEND_UNSUPPORTED;
  }

  int rcode = tjDecompressHeader(private.jpeg, private.data, private.len,
      &private.width, &private.height);
  if (rcode) {
    tjDestroy(private.jpeg);
    return BACKEND_UNSUPPORTED;
  }

  struct private *new_private = malloc(sizeof private);
  memcpy(new_private, &private, sizeof private);

  *src = imv_source_create(&vtable, new_private);
  return BACKEND_SUCCESS;
}

const struct imv_backend imv_backend_libjpeg = {
  .name = "libjpeg-turbo",
  .description = "Fast JPEG codec based on libjpeg. "
                 "This software is based in part on the work "
                 "of the Independent JPEG Group.",
  .website = "https://libjpeg-turbo.org/",
  .license = "The Modified BSD License",
  .open_path = &open_path,
  .open_memory = &open_memory,
};
