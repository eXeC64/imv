#ifndef IMV_CANVAS_H
#define IMV_CANVAS_H

#include <stdbool.h>

#include <pango/pangocairo.h>

struct imv_canvas;
struct imv_image;

enum upscaling_method {
  UPSCALING_LINEAR,
  UPSCALING_NEAREST_NEIGHBOUR,
  UPSCALING_METHOD_COUNT,
};

/* Create a canvas instance */
struct imv_canvas *imv_canvas_create(int width, int height);

/* Clean up a canvas */
void imv_canvas_free(struct imv_canvas *canvas);

/* Set the buffer size of the canvas */
void imv_canvas_resize(struct imv_canvas *canvas, int width, int height, double scale);

/* Blank the canvas to be empty and transparent */
void imv_canvas_clear(struct imv_canvas *canvas);

/* Set the current drawing color of the canvas */
void imv_canvas_color(struct imv_canvas *canvas, float r, float g, float b, float a);

/* Fill a rectangle on the canvas with the current color */
void imv_canvas_fill_rectangle(struct imv_canvas *canvas, int x, int y, int width, int height);

/* Fill the whole canvas with the current color */
void imv_canvas_fill(struct imv_canvas *canvas);

/* Fill the whole canvas with a chequerboard pattern */
void imv_canvas_fill_checkers(struct imv_canvas *canvas, int size);

/* Select the font to draw text with */
void imv_canvas_font(struct imv_canvas *canvas, const char *name, int size);

/* Prepare layout containing the given string, ready for rendering on the given
 * canvas. The caller is responsible for releasing it with a call to
 * g_object_unref */
PangoLayout *imv_canvas_make_layout(struct imv_canvas *canvas, const char *str);

/* Shows layout with at the specified coordinates */
void imv_canvas_show_layout(struct imv_canvas *canvas, int x, int y,
                            PangoLayout *layout);

/* Draw some text on the canvas, returns the width used in pixels */
int imv_canvas_printf(struct imv_canvas *canvas, int x, int y, const char *fmt, ...);

/* Blit the canvas to the current OpenGL framebuffer */
void imv_canvas_draw(struct imv_canvas *canvas);

/* Blit the given image to the current OpenGL framebuffer */
void imv_canvas_draw_image(struct imv_canvas *canvas, struct imv_image *image,
                           int x, int y, double scale,
                           double rotation, bool mirrored,
                           enum upscaling_method upscaling_method,
                           bool cache_invalidated);

#endif
