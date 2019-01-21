#include "backend_rsvg.h"
#include "backend.h"
#include "source.h"

#include <librsvg/rsvg.h>

/* Some systems like GNU/Hurd don't define PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void source_free(struct imv_source *src)
{
  free(src->name);
  src->name = NULL;

  free(src);
}

static struct imv_bitmap *to_imv_bitmap(GdkPixbuf *bitmap)
{
  struct imv_bitmap *bmp = malloc(sizeof(struct imv_bitmap));
  bmp->width = gdk_pixbuf_get_width(bitmap);
  bmp->height = gdk_pixbuf_get_height(bitmap);
  bmp->format = IMV_ARGB;
  size_t len = bmp->width * bmp->height * 4;
  bmp->data = malloc(len);
  memcpy(bmp->data, gdk_pixbuf_get_pixels(bitmap), len);
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


static void send_bitmap(struct imv_source *src, GdkPixbuf *bitmap)
{
  if (!src->callback) {
    fprintf(stderr, "imv_source(%s) has no callback configured. "
                    "Discarding result.\n", src->name);
    return;
  }

  struct imv_source_message msg;
  msg.source = src;
  msg.user_data = src->user_data;
  msg.bitmap = to_imv_bitmap(bitmap);
  msg.frametime = 0;
  msg.error = NULL;

  src->callback(&msg);
}

static void load_image(struct imv_source *src)
{
  GError *error;
  char path[PATH_MAX+8];
  snprintf(path, sizeof path, "file://%s", src->name);
  RsvgHandle *handle = rsvg_handle_new_from_file(path, &error);
  if (!handle) {
    report_error(src);
    return;
  }

  RsvgDimensionData dim;
  rsvg_handle_get_dimensions(handle, &dim);
  src->width = dim.width;
  src->height = dim.height;

  GdkPixbuf *buf = rsvg_handle_get_pixbuf(handle);
  if (!buf) {
    rsvg_handle_close(handle, &error);
    report_error(src);
    return;
  }

  rsvg_handle_close(handle, &error);
  send_bitmap(src, buf);
}

static enum backend_result open_path(const char *path, struct imv_source **src)
{
  /* Look for an <SVG> tag near the start of the file */
  char header[128];
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

  struct imv_source *source = calloc(1, sizeof(struct imv_source));
  source->name = strdup(path);

  source->width = 1024;
  source->height = 1024;
  source->num_frames = 1;
  source->next_frame = 1;
  source->load_first_frame = &load_image;
  source->load_next_frame = NULL;
  source->free = &source_free;
  source->callback = NULL;
  source->user_data = NULL;
  source->private = NULL;

  *src = source;
  return BACKEND_SUCCESS;
}

static void backend_free(struct imv_backend *backend)
{
  free(backend);
}

struct imv_backend *imv_backend_rsvg(void)
{
  struct imv_backend *backend = malloc(sizeof(struct imv_backend));
  backend->name = "rsvg (LGPL license)";
  backend->open_path = &open_path;
  backend->free = &backend_free;
  return backend;
}
