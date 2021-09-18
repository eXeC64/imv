#include "canvas.h"

#include "image.h"
#include "log.h"

#include <GL/gl.h>
#include <assert.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef IMV_BACKEND_LIBRSVG
#include <librsvg/rsvg.h>
#endif

// 16x16 chequerboard texture data
#define REPEAT8(...) __VA_ARGS__, __VA_ARGS__, __VA_ARGS__, __VA_ARGS__, __VA_ARGS__, __VA_ARGS__, __VA_ARGS__, __VA_ARGS__
unsigned char checkers_data[] = { REPEAT8(REPEAT8(0xCC, 0xCC, 0xCC, 0xFF), REPEAT8(0x80, 0x80, 0x80, 0xFF)),
                                  REPEAT8(REPEAT8(0x80, 0x80, 0x80, 0xFF), REPEAT8(0xCC, 0xCC, 0xCC, 0xFF)) };

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
  GLuint checkers_texture;
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

  glGenTextures(1, &canvas->checkers_texture);
  assert(canvas->checkers_texture);

  glBindTexture(GL_TEXTURE_2D, canvas->checkers_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 16);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16, 0, GL_RGBA,
               GL_UNSIGNED_INT_8_8_8_8_REV, checkers_data);

  canvas->width = width;
  canvas->height = height;

  return canvas;
}

void imv_canvas_free(struct imv_canvas *canvas)
{
  if (!canvas) {
    return;
  }
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
  glDeleteTextures(1, &canvas->checkers_texture);
  free(canvas);
}

void imv_canvas_resize(struct imv_canvas *canvas, int width, int height, double scale)
{
  cairo_destroy(canvas->cairo);
  cairo_surface_destroy(canvas->surface);

  canvas->width = width;
  canvas->height = height;

  canvas->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                               canvas->width, canvas->height);
  assert(canvas->surface);
  cairo_surface_set_device_scale(canvas->surface, scale, scale);
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

void imv_canvas_fill_checkers(struct imv_canvas *canvas, struct imv_image *image,
                              int bx, int by, double scale,
                              double rotation, bool mirrored)
{
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);

  glPushMatrix();
  glOrtho(0.0, viewport[2], viewport[3], 0.0, 0.0, 10.0);

  glBindTexture(GL_TEXTURE_2D, canvas->checkers_texture);
  glEnable(GL_TEXTURE_2D);

  const int left = bx;
  const int top = by;
  const int right = left + imv_image_width(image) * scale;
  const int bottom = top + imv_image_height(image) * scale;
  const int center_x = left + imv_image_width(image) * scale / 2;
  const int center_y = top + imv_image_height(image) * scale / 2;
  const float s = (right - left) / 16.0;
  const float t = s * imv_image_height(image) / imv_image_width(image);

  glTranslated(center_x, center_y, 0);
  if (mirrored) {
    glScaled(-1, 1, 1);
  }
  glRotated(rotation, 0, 0, 1);
  glTranslated(-center_x, -center_y, 0);

  glBegin(GL_TRIANGLE_FAN);
  glTexCoord2f(0, 0); glVertex2i(left, top);
  glTexCoord2f(s, 0); glVertex2i(right, top);
  glTexCoord2f(s, t); glVertex2i(right, bottom);
  glTexCoord2f(0, t); glVertex2i(left, bottom);
  glEnd();

  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);
  glPopMatrix();
}

void imv_canvas_font(struct imv_canvas *canvas, const char *name, int size)
{
  pango_font_description_set_family(canvas->font, name);
  pango_font_description_set_weight(canvas->font, PANGO_WEIGHT_NORMAL);
  pango_font_description_set_size(canvas->font, size * PANGO_SCALE);
}

PangoLayout *imv_canvas_make_layout(struct imv_canvas *canvas, const char *line)
{
  PangoLayout *layout = pango_cairo_create_layout(canvas->cairo);
  pango_layout_set_font_description(layout, canvas->font);
  pango_layout_set_text(layout, line, -1);

  return layout;
}

void imv_canvas_show_layout(struct imv_canvas *canvas, int x, int y,
                            PangoLayout *layout)
{
  cairo_move_to(canvas->cairo, x, y);
  pango_cairo_show_layout(canvas->cairo, layout);
}

int imv_canvas_printf(struct imv_canvas *canvas, int x, int y, const char *fmt, ...)
{
  char line[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof line, fmt, args);

  PangoLayout *layout = imv_canvas_make_layout(canvas, line);

  imv_canvas_show_layout(canvas, x, y, layout);

  PangoRectangle extents;
  pango_layout_get_pixel_extents(layout, NULL, &extents);

  g_object_unref(layout);

  va_end(args);
  return extents.width;
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
                        double rotation, bool mirrored,
                        enum upscaling_method upscaling_method,
                        bool cache_invalidated)
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

  GLint upscaling = 0;
  if (upscaling_method == UPSCALING_LINEAR) {
    upscaling = GL_LINEAR;
  } else if (upscaling_method == UPSCALING_NEAREST_NEIGHBOUR) {
    upscaling = GL_NEAREST;
  } else {
    imv_log(IMV_ERROR, "Unknown upscaling method: %d\n", upscaling_method);
    abort();
  }

  if (canvas->cache.bitmap != bitmap || cache_invalidated) {
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, upscaling);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, upscaling);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, bitmap->width);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, bitmap->width, bitmap->height,
        0, format, GL_UNSIGNED_INT_8_8_8_8_REV, bitmap->data);
  }
  canvas->cache.bitmap = bitmap;

  glEnable(GL_TEXTURE_RECTANGLE);

  glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, upscaling);

  const int left = bx;
  const int top = by;
  const int right = left + bitmap->width * scale;
  const int bottom = top + bitmap->height * scale;
  const int center_x = left + bitmap->width * scale / 2;
  const int center_y = top + bitmap->height * scale / 2;

  glTranslated(center_x, center_y, 0);
  if (mirrored) {
    glScaled(-1, 1, 1);
  }
  glRotated(rotation, 0, 0, 1);
  glTranslated(-center_x, -center_y, 0);

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
                           double rotation, bool mirrored,
                           enum upscaling_method upscaling_method,
                           bool cache_invalidated)
{
  struct imv_bitmap *bitmap = imv_image_get_bitmap(image);
  if (bitmap) {
    draw_bitmap(canvas, bitmap, x, y, scale, rotation, mirrored,
                upscaling_method, cache_invalidated);
    return;
  }

#ifdef IMV_BACKEND_LIBRSVG
  RsvgHandle *svg = imv_image_get_svg(image);
  if (svg) {
    imv_canvas_clear(canvas);
    cairo_translate(canvas->cairo, x, y);
    cairo_scale(canvas->cairo, scale, scale);
    cairo_translate(canvas->cairo, imv_image_width(image) / 2, imv_image_height(image) / 2);
    if (mirrored) {
      cairo_scale(canvas->cairo, -1, 1);
    }
    cairo_rotate(canvas->cairo, rotation * M_PI / 180.0);
    cairo_translate(canvas->cairo, -imv_image_width(image) / 2, -imv_image_height(image) / 2);
    rsvg_handle_render_cairo(svg, canvas->cairo);
    cairo_identity_matrix(canvas->cairo);
    imv_canvas_draw(canvas);
    return;
  }
#endif
}
