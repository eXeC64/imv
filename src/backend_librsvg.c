#include "backend.h"
#include "source.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <librsvg/rsvg.h>

/* Some systems like GNU/Hurd don't define PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct private {
  void *data;
  size_t len;
};

static void source_free(struct imv_source *src)
{
  pthread_mutex_lock(&src->busy);
  free(src->name);
  src->name = NULL;

  struct private *private = src->private;
  free(private);
  src->private = NULL;

  pthread_mutex_unlock(&src->busy);
  pthread_mutex_destroy(&src->busy);
  free(src);
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

static void send_svg(struct imv_source *src, RsvgHandle *handle)
{
  assert(src->callback);

  struct imv_source_message msg;
  msg.source = src;
  msg.user_data = src->user_data;
  msg.image = imv_image_create_from_svg(handle);
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

  RsvgHandle *handle = NULL;
  GError *error = NULL;

  struct private *private = src->private;
  if (private->data) {
    handle = rsvg_handle_new_from_data(private->data, private->len, &error);
  } else {
    char path[PATH_MAX+8];
    snprintf(path, sizeof path, "file://%s", src->name);
    handle = rsvg_handle_new_from_file(path, &error);
  }

  if (!handle) {
    report_error(src);
    return NULL;
  }

  RsvgDimensionData dim;
  rsvg_handle_get_dimensions(handle, &dim);
  src->width = dim.width;
  src->height = dim.height;

  send_svg(src, handle);
  return NULL;
}

static enum backend_result open_path(const char *path, struct imv_source **src)
{
  /* Look for an <SVG> tag near the start of the file */
  char header[4096];
  FILE *f = fopen(path, "rb");
  if (!f) {
    return BACKEND_BAD_PATH;
  }
  fread(header, 1, sizeof header, f);
  fclose(f);

  header[(sizeof header) - 1] = 0;
  if (!strstr(header, "<SVG") && !strstr(header, "<svg")) {
    return BACKEND_UNSUPPORTED;
  }

  struct private *private = malloc(sizeof *private);
  private->data = NULL;
  private->len = 0;

  struct imv_source *source = calloc(1, sizeof *source);
  source->name = strdup(path);

  source->width = 1024;
  source->height = 1024;
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

static enum backend_result open_memory(void *data, size_t len, struct imv_source **src)
{
  /* Look for an <SVG> tag near the start of the file */
  char header[4096];
  size_t header_len = sizeof header;
  if (header_len > len) {
    header_len = len;
  }
  memcpy(header, data, header_len);
  header[header_len - 1] = 0;
  if (!strstr(header, "<SVG") && !strstr(header, "<svg")) {
    return BACKEND_UNSUPPORTED;
  }

  struct private *private = malloc(sizeof *private);
  private->data = data;
  private->len = len;

  struct imv_source *source = calloc(1, sizeof *source);
  source->name = strdup("-");

  source->width = 1024;
  source->height = 1024;
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

const struct imv_backend imv_backend_librsvg = {
  .name = "libRSVG",
  .description = "SVG library developed by GNOME",
  .website = "https://wiki.gnome.org/Projects/LibRsvg",
  .license = "Lesser GNU Public License",
  .open_path = &open_path,
  .open_memory = &open_memory,
};

