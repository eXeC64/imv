#include "viewport.h"

#include <stdlib.h>

struct imv_viewport {
  double scale;
  struct {
    int width, height;
  } window; /* window dimensions */
  struct {
    int width, height;
  } buffer; /* rendering buffer dimensions */
  int x, y;
  double pan_factor_x, pan_factor_y;
  int redraw;
  int playing;
  int locked;
};

static void input_xy_to_render_xy(struct imv_viewport *view, int *x, int *y)
{
  *x *= view->buffer.width / view->window.width;
  *y *= view->buffer.height / view->window.height;
}

struct imv_viewport *imv_viewport_create(int window_width, int window_height,
                                         int buffer_width, int buffer_height)
{
  struct imv_viewport *view = malloc(sizeof *view);
  view->window.width = window_width;
  view->window.height = window_height;
  view->buffer.width = buffer_width;
  view->buffer.height = buffer_height;
  view->scale = 1;
  view->x = view->y = view->redraw = 0;
  view->pan_factor_x = view->pan_factor_y = 0.5;
  view->playing = 1;
  view->locked = 0;
  return view;
}

void imv_viewport_free(struct imv_viewport *view)
{
  free(view);
}

void imv_viewport_set_playing(struct imv_viewport *view, bool playing)
{
  view->playing = playing;
}

bool imv_viewport_is_playing(struct imv_viewport *view)
{
  return view->playing;
}

void imv_viewport_toggle_playing(struct imv_viewport *view)
{
  view->playing = !view->playing;
}

void imv_viewport_scale_to_actual(struct imv_viewport *view, const struct imv_image *image)
{
  view->scale = 1;
  view->redraw = 1;
  view->locked = 1;
  imv_viewport_center(view, image);
}

void imv_viewport_get_offset(struct imv_viewport *view, int *x, int *y)
{
  if(x) {
    *x = view->x;
  }
  if(y) {
    *y = view->y;
  }
}

void imv_viewport_get_scale(struct imv_viewport *view, double *scale)
{
  if(scale) {
    *scale = view->scale;
  }
}

void imv_viewport_set_default_pan_factor(struct imv_viewport *view, double pan_factor_x, double pan_factor_y)
{
  view->pan_factor_x = pan_factor_x;
  view->pan_factor_y = pan_factor_y;
}

void imv_viewport_keep_onscreen(struct imv_viewport *view,
    const struct imv_image *image)
{
  int w = (int)(imv_image_width(image) * view->scale);
  int h = (int)(imv_image_height(image) * view->scale);
  if (view->x < -w) {
    view->x = -w;
  }
  if (view->x > view->buffer.width) {
    view->x = view->buffer.width;
  }
  if (view->y < -h) {
    view->y = -h;
  }
  if (view->y > view->buffer.height) {
    view->y = view->buffer.height;
  }
}

void imv_viewport_move(struct imv_viewport *view, int x, int y,
    const struct imv_image *image)
{
  input_xy_to_render_xy(view, &x, &y);
  view->x += x;
  view->y += y;
  view->redraw = 1;
  view->locked = 1;
  imv_viewport_keep_onscreen(view, image);
}

void imv_viewport_move_relative(struct imv_viewport *view,
				int initial_x, int initial_y,
				int delta_x, int delta_y,
				struct imv_image *image)
{
  view->x = initial_x + delta_x;
  view->y = initial_y + delta_y;
  view->redraw = 1;
  view->locked = 1;
  imv_viewport_keep_onscreen(view, image);
}

void imv_viewport_zoom(struct imv_viewport *view, const struct imv_image *image,
                       enum imv_zoom_source src, int mouse_x, int mouse_y, int amount)
{
  double prev_scale = view->scale;
  int x, y;

  const int image_width = imv_image_width(image);
  const int image_height = imv_image_height(image);

  /* x and y cordinates are relative to the image */
  if(src == IMV_ZOOM_MOUSE || src == IMV_ZOOM_TOUCH) {
    input_xy_to_render_xy(view, &mouse_x, &mouse_y);
    x = mouse_x - view->x;
    y = mouse_y - view->y;
  } else {
    x = view->scale * image_width / 2;
    y = view->scale * image_height / 2;
  }

  const int scaled_width = image_width * view->scale;
  const int scaled_height = image_height * view->scale;
  const int ic_x = view->x + scaled_width/2;
  const int ic_y = view->y + scaled_height/2;
  const int wc_x = view->buffer.width/2;
  const int wc_y = view->buffer.height/2;

  if (src == IMV_ZOOM_TOUCH) {
    view->scale = amount / 100.0f;
  } else {
    double delta_scale = 0.04 * view->buffer.width * amount / image_width;
    view->scale += delta_scale;
  }

  const double min_scale = 0.1;
  const double max_scale = 100;
  if(view->scale > max_scale) {
    view->scale = max_scale;
  } else if (view->scale < min_scale) {
    view->scale = min_scale;
  }

  if(view->scale < prev_scale) {
    if(scaled_width < view->buffer.width) {
      x = scaled_width/2 - (ic_x - wc_x)*2;
    }
    if(scaled_height < view->buffer.height) {
      y = scaled_height/2 - (ic_y - wc_y)*2;
    }
  } else {
    if(scaled_width < view->buffer.width) {
      x = scaled_width/2;
    }
    if(scaled_height < view->buffer.height) {
      y = scaled_height/2;
    }
  }

  const double changeX = x - (x * (view->scale / prev_scale));
  const double changeY = y - (y * (view->scale / prev_scale));

  view->x += changeX;
  view->y += changeY;

  view->redraw = 1;
  view->locked = 1;
}

void imv_viewport_center(struct imv_viewport *view, const struct imv_image *image)
{
  const int image_width = imv_image_width(image);
  const int image_height = imv_image_height(image);

  view->x = view->buffer.width - image_width * view->scale;
  view->y = view->buffer.height - image_height * view->scale;

  if (view->x > 0) {
    /* Image is smaller than the window. Center the image */
    view->x *= 0.5;
  } else {
    view->x *= view->pan_factor_x;
  }

  if (view->y > 0) {
    /* Image is smaller than the window. Center the image */
    view->y *= 0.5;
  } else {
    view->y *= view->pan_factor_y;
  }

  view->locked = 1;
  view->redraw = 1;
}

double imv_viewport_get_scale_to_window(struct imv_viewport *view,
					const struct imv_image *image)
{
  const int image_width = imv_image_width(image);
  const int image_height = imv_image_height(image);
  const double window_aspect = (double)view->buffer.width / (double)view->buffer.height;
  const double image_aspect = (double)image_width / (double)image_height;

  if(window_aspect > image_aspect) {
    /* Image will become too tall before it becomes too wide */
    return (double)view->buffer.height / (double)image_height;
  } else {
    /* Image will become too wide before it becomes too tall */
    return (double)view->buffer.width / (double)image_width;
  }
}

void imv_viewport_scale_to_window(struct imv_viewport *view, const struct imv_image *image)
{
  view->scale = imv_viewport_get_scale_to_window(view, image);
  imv_viewport_center(view, image);
  view->locked = 0;
}

void imv_viewport_crop_to_window(struct imv_viewport *view, const struct imv_image *image)
{
  const int image_width = imv_image_width(image);
  const int image_height = imv_image_height(image);
  const double window_aspect = (double)view->buffer.width / (double)view->buffer.height;
  const double image_aspect = (double)image_width / (double)image_height;

  /* Scale the image so that it fills the whole window */
  if(window_aspect > image_aspect) {
    view->scale = (double)view->buffer.width / (double)image_width;
  } else {
    view->scale = (double)view->buffer.height / (double)image_height;
  }

  imv_viewport_center(view, image);
  view->locked = 0;
}

void imv_viewport_set_redraw(struct imv_viewport *view)
{
  view->redraw = 1;
}

void imv_viewport_rescale(struct imv_viewport *view, const struct imv_image *image,
                          enum scaling_mode scaling_mode) {
  if (scaling_mode == SCALING_NONE ||
      (scaling_mode == SCALING_DOWN
       && view->buffer.width > imv_image_width(image)
       && view->buffer.height > imv_image_height(image))) {
    imv_viewport_scale_to_actual(view, image);
  } else if (scaling_mode == SCALING_CROP) {
    imv_viewport_crop_to_window(view, image);
  } else {
    imv_viewport_scale_to_window(view, image);
  }
}

void imv_viewport_update(struct imv_viewport *view,
                         int window_width, int window_height,
                         int buffer_width, int buffer_height,
                         struct imv_image *image,
                         enum scaling_mode scaling_mode)
{
  view->window.width = window_width;
  view->window.height = window_height;
  view->buffer.width = buffer_width;
  view->buffer.height = buffer_height;

  view->redraw = 1;
  if(view->locked) {
    return;
  }

  imv_viewport_center(view, image);
  imv_viewport_rescale(view, image, scaling_mode);
}

int imv_viewport_needs_redraw(struct imv_viewport *view)
{
  int redraw = 0;
  if(view->redraw) {
    redraw = 1;
    view->redraw = 0;
  }
  return redraw;
}


/* vim:set ts=2 sts=2 sw=2 et: */
