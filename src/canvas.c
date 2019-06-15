#include "canvas.h"

#include "image.h"
#include "log.h"

#include <GLFW/glfw3.h>
#include <assert.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef IMV_BACKEND_LIBRSVG
#include <librsvg/rsvg.h>
#endif

struct imv_canvas {
  cairo_surface_t *surface;
  cairo_t *cairo;
  PangoFontDescription *font;
  GLuint texture;
  int width;
  int height;
  struct {
    struct imv_bitmap *bitmap;
    GLuint texture;
  } cache;
};

struct imv_canvas *imv_canvas_create(int width, int height)
{
  struct imv_canvas *canvas = calloc(1, sizeof *canvas);
  canvas->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                               width, height);
  assert(canvas->surface);
  canvas->cairo = cairo_create(canvas->surface);
  assert(canvas->cairo);

  canvas->font = pango_font_description_new();
  assert(canvas->font);

  glGenTextures(1, &canvas->texture);
  assert(canvas->texture);

  canvas->width = width;
  canvas->height = height;

  return canvas;
}

void imv_canvas_free(struct imv_canvas *canvas)
{
  pango_font_description_free(canvas->font);
  canvas->font = NULL;
  cairo_destroy(canvas->cairo);
  canvas->cairo = NULL;
  cairo_surface_destroy(canvas->surface);
  canvas->surface = NULL;
  glDeleteTextures(1, &canvas->texture);
  if (canvas->cache.texture) {
    glDeleteTextures(1, &canvas->cache.texture);
  }
  free(canvas);
}

void imv_canvas_resize(struct imv_canvas *canvas, int width, int height)
{
  cairo_destroy(canvas->cairo);
  cairo_surface_destroy(canvas->surface);

  canvas->width = width;
  canvas->height = height;

  canvas->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                               canvas->width, canvas->height);
  assert(canvas->surface);
  canvas->cairo = cairo_create(canvas->surface);
  assert(canvas->cairo);
}

void imv_canvas_clear(struct imv_canvas *canvas)
{
  cairo_save(canvas->cairo);
  cairo_set_source_rgba(canvas->cairo, 0, 0, 0, 0);
  cairo_set_operator(canvas->cairo, CAIRO_OPERATOR_SOURCE);
  cairo_paint(canvas->cairo);
  cairo_restore(canvas->cairo);
}

void imv_canvas_color(struct imv_canvas *canvas, float r, float g, float b, float a)
{
  cairo_set_source_rgba(canvas->cairo, r, g, b, a);
}

void imv_canvas_fill_rectangle(struct imv_canvas *canvas, int x, int y, int width, int height)
{
  cairo_rectangle(canvas->cairo, x, y, width, height);
  cairo_fill(canvas->cairo);
}

void imv_canvas_fill(struct imv_canvas *canvas)
{
  cairo_rectangle(canvas->cairo, 0, 0, canvas->width, canvas->height);
  cairo_fill(canvas->cairo);
}

void imv_canvas_fill_checkers(struct imv_canvas *canvas, int size)
{
  for (int x = 0; x < canvas->width; x += size) {
    for (int y = 0; y < canvas->height; y += size) {
      float color = ((x/size + y/size) % 2 == 0) ? 0.25 : 0.75;
      cairo_set_source_rgba(canvas->cairo, color, color, color, 1);
      cairo_rectangle(canvas->cairo, x, y, size, size);
      cairo_fill(canvas->cairo);
    }
  }
}

void imv_canvas_font(struct imv_canvas *canvas, const char *name, int size)
{
  pango_font_description_set_family(canvas->font, name);
  pango_font_description_set_weight(canvas->font, PANGO_WEIGHT_NORMAL);
  pango_font_description_set_absolute_size(canvas->font, size * PANGO_SCALE);
}

void imv_canvas_printf(struct imv_canvas *canvas, int x, int y, const char *fmt, ...)
{
  char line[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof line, fmt, args);

  PangoLayout *layout = pango_cairo_create_layout(canvas->cairo);
  pango_layout_set_font_description(layout, canvas->font);
  pango_layout_set_text(layout, line, -1);

  cairo_move_to(canvas->cairo, x, y);
  pango_cairo_show_layout(canvas->cairo, layout);
  g_object_unref(layout);

  va_end(args);
}

void imv_canvas_draw(struct imv_canvas *canvas)
{
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  glPushMatrix();
  glOrtho(0.0, 1.0, 1.0, 0.0, 0.0, 10.0);

  void *data = cairo_image_surface_get_data(canvas->surface);

  glEnable(GL_TEXTURE_RECTANGLE);
  glBindTexture(GL_TEXTURE_RECTANGLE, canvas->texture);

  glPixelStorei(GL_UNPACK_ROW_LENGTH, canvas->width);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, canvas->width, canvas->height,
               0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin(GL_TRIANGLE_FAN);
  glTexCoord2i(0,             0);              glVertex2i(0.0, 0.0);
  glTexCoord2i(canvas->width, 0);              glVertex2i(1.0, 0.0);
  glTexCoord2i(canvas->width, canvas->height); glVertex2i(1.0, 1.0);
  glTexCoord2i(0,             canvas->height); glVertex2i(0.0, 1.0);
  glEnd();
  glDisable(GL_BLEND);

  glBindTexture(GL_TEXTURE_RECTANGLE, 0);
  glDisable(GL_TEXTURE_RECTANGLE);
  glPopMatrix();
}

struct imv_bitmap *imv_image_get_bitmap(const struct imv_image *image);

static int convert_pixelformat(enum imv_pixelformat fmt)
{
  /* opengl uses RGBA order, not ARGB, so we get it to
   * flip the bytes around so ARGB -> BGRA
   */
  if (fmt == IMV_ARGB) {
    return GL_BGRA;
  } else if (fmt == IMV_ABGR) {
    return GL_RGBA;
  } else {
    imv_log(IMV_WARNING, "Unknown pixel format. Defaulting to ARGB\n");
    return GL_BGRA;
  }
}

static void draw_bitmap(struct imv_canvas *canvas,
                        struct imv_bitmap *bitmap,
                        int bx, int by, double scale,
                        enum upscaling_method upscaling_method)
{
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);

  glPushMatrix();
  glOrtho(0.0, viewport[2], viewport[3], 0.0, 0.0, 10.0);

  if (!canvas->cache.texture) {
    glGenTextures(1, &canvas->cache.texture);
  }

  const int format = convert_pixelformat(bitmap->format);

  glBindTexture(GL_TEXTURE_RECTANGLE, canvas->cache.texture);

  if (canvas->cache.bitmap != bitmap) {
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, bitmap->width);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, bitmap->width, bitmap->height,
        0, format, GL_UNSIGNED_INT_8_8_8_8_REV, bitmap->data);
  }
  canvas->cache.bitmap = bitmap;

  glEnable(GL_TEXTURE_RECTANGLE);

  if (upscaling_method == UPSCALING_LINEAR) {
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  } else if (upscaling_method == UPSCALING_NEAREST_NEIGHBOUR) {
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  } else {
    imv_log(IMV_ERROR, "Unknown upscaling method: %d\n", upscaling_method);
    abort();
  }

  const int left = bx;
  const int top = by;
  const int right = left + bitmap->width * scale;
  const int bottom = top + bitmap->height * scale;

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBegin(GL_TRIANGLE_FAN);
  glTexCoord2i(0,             0);              glVertex2i(left, top);
  glTexCoord2i(bitmap->width, 0);              glVertex2i(right, top);
  glTexCoord2i(bitmap->width, bitmap->height); glVertex2i(right, bottom);
  glTexCoord2i(0,             bitmap->height); glVertex2i(left, bottom);
  glEnd();

  glDisable(GL_BLEND);

  glBindTexture(GL_TEXTURE_RECTANGLE, 0);
  glDisable(GL_TEXTURE_RECTANGLE);
  glPopMatrix();
}

#ifdef IMV_BACKEND_LIBRSVG
RsvgHandle *imv_image_get_svg(const struct imv_image *image);
#endif

void imv_canvas_draw_image(struct imv_canvas *canvas, struct imv_image *image,
                           int x, int y, double scale,
                           enum upscaling_method upscaling_method)
{
  struct imv_bitmap *bitmap = imv_image_get_bitmap(image);
  if (bitmap) {
    draw_bitmap(canvas, bitmap, x, y, scale, upscaling_method);
    return;
  }

#ifdef IMV_BACKEND_LIBRSVG
  RsvgHandle *svg = imv_image_get_svg(image);
  if (svg) {
    imv_canvas_clear(canvas);
    cairo_translate(canvas->cairo, x, y);
    cairo_scale(canvas->cairo, scale, scale);
    rsvg_handle_render_cairo(svg, canvas->cairo);
    cairo_identity_matrix(canvas->cairo);
    imv_canvas_draw(canvas);
    return;
  }
#endif
}
