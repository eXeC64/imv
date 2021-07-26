#ifndef IMV_WINDOW_H
#define IMV_WINDOW_H

#include <stdbool.h>

struct imv_window;

enum imv_event_type {
  IMV_EVENT_CLOSE,
  IMV_EVENT_RESIZE,
  IMV_EVENT_KEYBOARD,
  IMV_EVENT_KEYBOARD_MODS,
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
      double scale;
    } resize;
    struct {
      int scancode;
      char *keyname;
      char *description;
      char *text;
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

/* Create a new window */
struct imv_window *imv_window_create(int w, int h, const char *title);

/* Clean up an imv_window instance */
void imv_window_free(struct imv_window *window);

/* Clears the window with the given colour */
void imv_window_clear(struct imv_window *window, unsigned char r,
    unsigned char g, unsigned char b);

/* Get the logical/event/window manager size of the window */
void imv_window_get_size(struct imv_window *window, int *w, int *h);

/* Get the pixel dimensions that the window is rendered at */
void imv_window_get_framebuffer_size(struct imv_window *window, int *w, int *h);

/* Set the window's title */
void imv_window_set_title(struct imv_window *window, const char *title);

/* Returns true if the window is fullscreen */
bool imv_window_is_fullscreen(struct imv_window *window);

/* Set the window's fullscreen state */
void imv_window_set_fullscreen(struct imv_window *window, bool fullscreen);

/* Check whether a given mouse button is pressed */
bool imv_window_get_mouse_button(struct imv_window *window, int button);

/* Gets the mouse's position */
void imv_window_get_mouse_position(struct imv_window *window, double *x, double *y);

/* Swap the framebuffers. Present anything rendered since the last call. */
void imv_window_present(struct imv_window *window);

/* Blocks until an event is received, or the timeout (in seconds) expires */
void imv_window_wait_for_event(struct imv_window *window, double timeout);

/* Push an event to the event queue. An internal copy of the event is made.
 * Wakes up imv_window_wait_for_event */
void imv_window_push_event(struct imv_window *window, struct imv_event *e);

typedef void (*imv_event_handler)(void *data, const struct imv_event *e);

/* When called, the handler function is called for each event on the event
 * queue */
void imv_window_pump_events(struct imv_window *window, imv_event_handler handler, void *data);

#endif
