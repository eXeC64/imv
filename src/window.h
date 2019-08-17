#ifndef IMV_WINDOW_H
#define IMV_WINDOW_H

#include <stdbool.h>

struct imv_window;

enum imv_event_type {
  IMV_EVENT_CLOSE,
  IMV_EVENT_RESIZE,
  IMV_EVENT_KEYBOARD,
  IMV_EVENT_MOUSE_MOTION,
  IMV_EVENT_MOUSE_BUTTON,
  IMV_EVENT_MOUSE_SCROLL,
  IMV_EVENT_CUSTOM
};

struct imv_event {
  enum imv_event_type type;
  union {
    struct {
      int width;
      int height;
      int buffer_width;
      int buffer_height;
    } resize;
    struct {
      int scancode;
      bool pressed;
    } keyboard;
    struct {
      double x, y, dx, dy;
    } mouse_motion;
    struct {
      int button;
      bool pressed;
    } mouse_button;
    struct {
      double dx, dy;
    } mouse_scroll;
    void *custom;
  } data;
};

struct imv_window *imv_window_create(int w, int h, const char *title);

void imv_window_free(struct imv_window *window);

void imv_window_clear(struct imv_window *window, unsigned char r,
    unsigned char g, unsigned char b);

void imv_window_get_size(struct imv_window *window, int *w, int *h);

void imv_window_get_framebuffer_size(struct imv_window *window, int *w, int *h);

void imv_window_set_title(struct imv_window *window, const char *title);

bool imv_window_is_fullscreen(struct imv_window *window);

void imv_window_set_fullscreen(struct imv_window *window, bool fullscreen);

bool imv_window_get_mouse_button(struct imv_window *window, int button);

void imv_window_get_mouse_position(struct imv_window *window, double *x, double *y);

void imv_window_present(struct imv_window *window);

void imv_window_wait_for_event(struct imv_window *window, double timeout);

void imv_window_push_event(struct imv_window *window, struct imv_event *e);

typedef void (*imv_event_handler)(void *data, const struct imv_event *e);

void imv_window_pump_events(struct imv_window *window, imv_event_handler handler, void *data);

const char *imv_window_get_keymap(struct imv_window *window);

#endif
