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
      int x, y, dx, dy;
    } mouse_motion;
    struct {
      int button;
      bool pressed;
    } mouse_button;
    void *custom;
  } data;
};

struct imv_window *imv_window_create(int w, int h, const char *title);

void imv_window_free(struct imv_window *window);

void imv_window_get_size(struct imv_window *window, int *w, int *h);

void imv_window_get_framebuffer_size(struct imv_window *window, int *w, int *h);

void imv_window_set_title(struct imv_window *window, const char *title);

void imv_window_present(struct imv_window *window);

void imv_window_resize(struct imv_window *window, int w, int h);

void imv_window_wait_for_event(struct imv_window *window, double timeout);

void imv_window_push_event(struct imv_window *window, struct imv_event *e);

typedef void (*imv_event_handler)(void *data, const struct imv_event *e);

void imv_window_pump_events(struct imv_window *window, imv_event_handler handler, void *data);

#endif
