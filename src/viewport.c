#include "viewport.h"

struct imv_viewport {
  SDL_Window *window;
  SDL_Renderer *renderer;
  double scale;
  int x, y;
  int fullscreen;
  int redraw;
  int playing;
  int locked;
};

static void input_xy_to_render_xy(struct imv_viewport *view, int *x, int *y)
{
  int ww, wh, rw, rh;
  SDL_GetWindowSize(view->window, &ww, &wh);
  SDL_GetRendererOutputSize(view->renderer, &rw, &rh);
  *x *= rw / ww;
  *y *= rh / wh;
}

struct imv_viewport *imv_viewport_create(SDL_Window *window, SDL_Renderer *renderer)
{
  struct imv_viewport *view = malloc(sizeof *view);
  view->window = window;
  view->renderer = renderer;
  view->scale = 1;
  view->x = view->y = view->fullscreen = view->redraw = 0;
  view->playing = 1;
  view->locked = 0;
  return view;
}

void imv_viewport_free(struct imv_viewport *view)
{
  free(view);
}

void imv_viewport_toggle_fullscreen(struct imv_viewport *view)
{
  if(view->fullscreen) {
    SDL_SetWindowFullscreen(view->window, 0);
    view->fullscreen = 0;
  } else {
    SDL_SetWindowFullscreen(view->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    view->fullscreen = 1;
  }
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

void imv_viewport_move(struct imv_viewport *view, int x, int y,
    const struct imv_image *image)
{
  input_xy_to_render_xy(view, &x, &y);
  view->x += x;
  view->y += y;
  view->redraw = 1;
  view->locked = 1;
  int w = (int)(imv_image_width(image) * view->scale);
  int h = (int)(imv_image_height(image) * view->scale);
  int ww, wh;
  SDL_GetRendererOutputSize(view->renderer, &ww, &wh);
  if (view->x < -w) {
    view->x = -w;
  }
  if (view->x > ww) {
    view->x = ww;
  }
  if (view->y < -h) {
    view->y = -h;
  }
  if (view->y > wh) {
    view->y = wh;
  }
}

void imv_viewport_zoom(struct imv_viewport *view, const struct imv_image *image, enum imv_zoom_source src, int amount)
{
  double prev_scale = view->scale;
  int x, y, ww, wh;
  SDL_GetRendererOutputSize(view->renderer, &ww, &wh);

  const int image_width = imv_image_width(image);
  const int image_height = imv_image_height(image);

  /* x and y cordinates are relative to the image */
  if(src == IMV_ZOOM_MOUSE) {
    SDL_GetMouseState(&x, &y);
    input_xy_to_render_xy(view, &x, &y);
    x -= view->x;
    y -= view->y;
  } else {
    x = view->scale * image_width / 2;
    y = view->scale * image_height / 2;
  }

  const int scaled_width = image_width * view->scale;
  const int scaled_height = image_height * view->scale;
  const int ic_x = view->x + scaled_width/2;
  const int ic_y = view->y + scaled_height/2;
  const int wc_x = ww/2;
  const int wc_y = wh/2;

  double delta_scale = 0.04 * ww * amount / image_width;
  view->scale += delta_scale;

  const double min_scale = 0.1;
  const double max_scale = 100;
  if(view->scale > max_scale) {
    view->scale = max_scale;
  } else if (view->scale < min_scale) {
    view->scale = min_scale;
  }

  if(view->scale < prev_scale) {
    if(scaled_width < ww) {
      x = scaled_width/2 - (ic_x - wc_x)*2;
    }
    if(scaled_height < wh) {
      y = scaled_height/2 - (ic_y - wc_y)*2;
    }
  } else {
    if(scaled_width < ww) {
      x = scaled_width/2;
    }
    if(scaled_height < wh) {
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
  int ww, wh;
  SDL_GetRendererOutputSize(view->renderer, &ww, &wh);

  const int image_width = imv_image_width(image);
  const int image_height = imv_image_height(image);

  view->x = (ww - image_width * view->scale) / 2;
  view->y = (wh - image_height * view->scale) / 2;

  view->locked = 1;
  view->redraw = 1;
}

void imv_viewport_scale_to_window(struct imv_viewport *view, const struct imv_image *image)
{
  int ww, wh;
  SDL_GetRendererOutputSize(view->renderer, &ww, &wh);

  const int image_width = imv_image_width(image);
  const int image_height = imv_image_height(image);
  const double window_aspect = (double)ww / (double)wh;
  const double image_aspect = (double)image_width / (double)image_height;

  if(window_aspect > image_aspect) {
    /* Image will become too tall before it becomes too wide */
    view->scale = (double)wh / (double)image_height;
  } else {
    /* Image will become too wide before it becomes too tall */
    view->scale = (double)ww / (double)image_width;
  }

  imv_viewport_center(view, image);
  view->locked = 0;
}

void imv_viewport_set_redraw(struct imv_viewport *view)
{
  view->redraw = 1;
}

void imv_viewport_set_title(struct imv_viewport *view, char* title)
{
  SDL_SetWindowTitle(view->window, title);
}

void imv_viewport_update(struct imv_viewport *view, struct imv_image *image)
{
  view->redraw = 1;
  if(view->locked) {
    return;
  }

  imv_viewport_center(view, image);
  imv_viewport_scale_to_window(view, image);
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
