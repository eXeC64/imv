#include "backend_libjpeg.h"
#include "backend.h"
#include "source.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef IMV_BACKEND_LIBJPEG

#include <turbojpeg.h>

struct private {
  int fd;
  void *data;
  size_t len;
  tjhandle jpeg;
};

static void source_free(struct imv_source *src)
{
  pthread_mutex_lock(&src->busy);
  free(src->name);
  src->name = NULL;

  struct private *private = src->private;
  tjDestroy(private->jpeg);
  if (private->fd >= 0) {
    munmap(private->data, private->len);
    close(private->fd);
  } else {
    free(private->data);
  }
  private->data = NULL;

  free(src->private);
  src->private = NULL;

  pthread_mutex_unlock(&src->busy);
  pthread_mutex_destroy(&src->busy);
  free(src);
}

static struct imv_bitmap *to_imv_bitmap(int width, int height, void *bitmap)
{
  struct imv_bitmap *bmp = malloc(sizeof(struct imv_bitmap));
  bmp->width = width;
  bmp->height = height;
  bmp->format = IMV_ABGR;
  bmp->data = bitmap;
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

static void send_bitmap(struct imv_source *src, void *bitmap)
{
  if (!src->callback) {
    fprintf(stderr, "imv_source(%s) has no callback configured. "
                    "Discarding result.\n", src->name);
    return;
  }

  struct imv_source_message msg;
  msg.source = src;
  msg.user_data = src->user_data;
  msg.bitmap = to_imv_bitmap(src->width, src->height, bitmap);
  msg.frametime = 0;
  msg.error = NULL;

  pthread_mutex_unlock(&src->busy);
  src->callback(&msg);
}

static int load_image(struct imv_source *src)
{
  /* Don't run if this source is already active */
  if (pthread_mutex_trylock(&src->busy)) {
    return -1;
  }

  struct private *private = src->private;

  void *bitmap = malloc(src->height * src->width * 4);
  int rcode = tjDecompress2(private->jpeg, private->data, private->len,
      bitmap, src->width, 0, src->height, TJPF_RGBA, TJFLAG_FASTDCT);

  if (rcode) {
    free(bitmap);
    report_error(src);
    return -1;
  }

  send_bitmap(src, bitmap);
  return 0;
}

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

  int width, height;
  int rcode = tjDecompressHeader(private.jpeg, private.data, private.len,
      &width, &height);
  if (rcode) {
    tjDestroy(private.jpeg);
    munmap(private.data, private.len);
    close(private.fd);
    return BACKEND_UNSUPPORTED;
  }

  struct imv_source *source = calloc(1, sizeof(struct imv_source));
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
  struct private private;

  private.fd = -1;
  private.data = data;
  private.len = len;

  private.jpeg = tjInitDecompress();
  if (!private.jpeg) {
    return BACKEND_UNSUPPORTED;
  }

  int width, height;
  int rcode = tjDecompressHeader(private.jpeg, private.data, private.len,
      &width, &height);
  if (rcode) {
    tjDestroy(private.jpeg);
    return BACKEND_UNSUPPORTED;
  }

  struct imv_source *source = calloc(1, sizeof(struct imv_source));
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
  source->private = malloc(sizeof private);
  memcpy(source->private, &private, sizeof private);

  *src = source;
  return BACKEND_SUCCESS;
}

const struct imv_backend libjpeg_backend = {
  .name = "libjpeg-turbo",
  .description = "Fast JPEG codec based on libjpeg. "
                 "This software is based in part on the work "
                 "of the Independent JPEG Group.",
  .website = "https://libjpeg-turbo.org/",
  .license = "The Modified BSD License",
  .open_path = &open_path,
  .open_memory = &open_memory,
};

const struct imv_backend *imv_backend_libjpeg(void)
{
  return &libjpeg_backend;
}

#else

const struct imv_backend *imv_backend_libjpeg(void)
{
  return NULL;
}

#endif
