#include "window.h"

#include <stdbool.h>
#include <unistd.h>

struct imv_window *imv_window_create(int w, int h, const char *title)
{
  (void)w;
  (void)h;
  (void)title;
  return NULL;
}

void imv_window_free(struct imv_window *window)
{
  (void)window;
}

void imv_window_clear(struct imv_window *window, unsigned char r,
    unsigned char g, unsigned char b)
{
  (void)window;
  (void)r;
  (void)g;
  (void)b;
}

void imv_window_get_size(struct imv_window *window, int *w, int *h)
{
  (void)window;
  (void)w;
  (void)h;
}

void imv_window_get_framebuffer_size(struct imv_window *window, int *w, int *h)
{
  (void)window;
  (void)w;
  (void)h;
}

void imv_window_set_title(struct imv_window *window, const char *title)
{
  (void)window;
  (void)title;
}

bool imv_window_is_fullscreen(struct imv_window *window)
{
  (void)window;
  return false;
}

void imv_window_set_fullscreen(struct imv_window *window, bool fullscreen)
{
  (void)window;
  (void)fullscreen;
}

bool imv_window_get_mouse_button(struct imv_window *window, int button)
{
  (void)window;
  (void)button;
  return false;
}

void imv_window_get_mouse_position(struct imv_window *window, double *x, double *y)
{
  (void)window;
  (void)x;
  (void)y;
}

void imv_window_present(struct imv_window *window)
{
  (void)window;
}

void imv_window_wait_for_event(struct imv_window *window, double timeout)
{
  (void)window;
  (void)timeout;
}

void imv_window_push_event(struct imv_window *window, struct imv_event *e)
{
  (void)window;
  (void)e;
}

void imv_window_pump_events(struct imv_window *window, imv_event_handler handler, void *data)
{
  (void)window;
  (void)handler;
  (void)data;
}

const char *imv_window_get_keymap(struct imv_window *window)
{
  (void)window;
  return NULL;
}
