#include "window.h"

#include <assert.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include "xdg-shell-client-protocol.h"

struct imv_window {
  struct wl_display    *wl_display;
  struct wl_registry   *wl_registry;
  struct wl_compositor *wl_compositor;
  struct wl_surface    *wl_surface;
  struct xdg_wm_base   *wl_xdg;
  struct xdg_surface   *wl_xdg_surface;
  struct xdg_toplevel  *wl_xdg_toplevel;
  struct wl_seat       *wl_seat;
  struct wl_keyboard   *wl_keyboard;
  struct wl_pointer    *wl_pointer;
  EGLDisplay           egl_display;
  EGLContext           egl_context;
  EGLSurface           egl_surface;
  struct wl_egl_window *egl_window;

  int display_fd;
  int event_fd;

  int width;
  int height;
  bool fullscreen;
  int scale;

  struct {
    struct {
      double last;
      double current;
    } x;
    struct {
      double last;
      double current;
    } y;
    struct {
      bool last;
      bool current;
    } mouse1;
    struct {
      double dx;
      double dy;
    } scroll;
  } pointer;

  struct {
    struct imv_event *queue;
    size_t len;
    size_t cap;
    pthread_mutex_t mutex;
  } events;
};

static void
handle_ping_xdg_wm_base(void *data, struct xdg_wm_base *xdg, uint32_t serial)
{
  (void)data;
  xdg_wm_base_pong(xdg, serial);
}

static const struct xdg_wm_base_listener shell_listener_xdg = {
  .ping = handle_ping_xdg_wm_base
};

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard,
    uint32_t format, int32_t fd, uint32_t size)
{
  (void)data;
  (void)keyboard;
  (void)format;
  (void)fd;
  (void)size;
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard,
    uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
  (void)data;
  (void)keyboard;
  (void)serial;
  (void)surface;
  (void)keys;
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard,
    uint32_t serial, struct wl_surface *surface)
{
  (void)data;
  (void)keyboard;
  (void)serial;
  (void)surface;
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard,
    uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
  (void)serial;
  (void)keyboard;
  (void)time;

  struct imv_window *window = data;
  struct imv_event e = {
    .type = IMV_EVENT_KEYBOARD,
    .data = {
      .keyboard = {
        .scancode = key,
        .pressed = state
      }
    }
  };
  imv_window_push_event(window, &e);
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
    uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
    uint32_t mods_locked, uint32_t group)
{
  (void)data;
  (void)keyboard;
  (void)serial;
  (void)mods_depressed;
  (void)mods_latched;
  (void)mods_locked;
  (void)group;
}

void keyboard_repeat(void *data, struct wl_keyboard *keyboard,
    int32_t rate, int32_t delay)
{
  (void)data;
  (void)keyboard;
  (void)rate;
  (void)delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
  .keymap = keyboard_keymap,
  .enter = keyboard_enter,
  .leave = keyboard_leave,
  .key = keyboard_key,
  .modifiers = keyboard_modifiers,
  .repeat_info = keyboard_repeat
};

static void pointer_enter(void *data, struct wl_pointer *pointer,
    uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x,
    wl_fixed_t surface_y)
{
  (void)pointer;
  (void)serial;
  (void)surface;

  struct imv_window *window = data;
  window->pointer.x.last = wl_fixed_to_double(surface_x);
  window->pointer.y.last = wl_fixed_to_double(surface_y);
  window->pointer.x.current = wl_fixed_to_double(surface_x);
  window->pointer.y.current = wl_fixed_to_double(surface_y);
}

static void pointer_leave(void *data, struct wl_pointer *pointer,
    uint32_t serial, struct wl_surface *surface)
{
  (void)data;
  (void)pointer;
  (void)serial;
  (void)surface;
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
    uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
  (void)pointer;
  (void)time;

  struct imv_window *window = data;
  window->pointer.x.current = wl_fixed_to_double(surface_x);
  window->pointer.y.current = wl_fixed_to_double(surface_y);
}

static void pointer_button(void *data, struct wl_pointer *pointer,
    uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
  (void)pointer;
  (void)serial;
  (void)time;

  struct imv_window *window = data;
  const uint32_t MOUSE1 = 0x110;
  if (button == MOUSE1) {
    window->pointer.mouse1.current = state;
  }
}

static void pointer_axis(void *data, struct wl_pointer *pointer,
    uint32_t time, uint32_t axis, wl_fixed_t value)
{
  (void)pointer;
  (void)time;

  struct imv_window *window = data;
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    window->pointer.scroll.dy += wl_fixed_to_double(value);
  } else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
    window->pointer.scroll.dx += wl_fixed_to_double(value);
  }
}

static void pointer_frame(void *data, struct wl_pointer *pointer)
{
  (void)pointer;

  struct imv_window *window = data;

  int dx = window->pointer.x.current - window->pointer.x.last;
  int dy = window->pointer.y.current - window->pointer.y.last;
  window->pointer.x.last = window->pointer.x.current;
  window->pointer.y.last = window->pointer.y.current;
  if (dx || dy) {
    struct imv_event e = {
      .type = IMV_EVENT_MOUSE_MOTION,
      .data = {
        .mouse_motion = {
          .x = window->pointer.x.current,
          .y = window->pointer.y.current,
          .dx = dx,
          .dy = dy,
        }
      }
    };
    imv_window_push_event(window, &e);
  }

  if (window->pointer.mouse1.current != window->pointer.mouse1.last) {
    window->pointer.mouse1.last = window->pointer.mouse1.current;
    struct imv_event e = {
      .type = IMV_EVENT_MOUSE_BUTTON,
      .data = {
        .mouse_button = {
          .button = 1,
          .pressed = window->pointer.mouse1.current
        }
      }
    };
    imv_window_push_event(window, &e);
  }

  if (window->pointer.scroll.dx || window->pointer.scroll.dy) {
    struct imv_event e = {
      .type = IMV_EVENT_MOUSE_SCROLL,
      .data = {
        .mouse_scroll = {
          .dx = window->pointer.scroll.dx,
          .dy = window->pointer.scroll.dy
        }
      }
    };
    imv_window_push_event(window, &e);
    window->pointer.scroll.dx = 0;
    window->pointer.scroll.dy = 0;
  }

}

static void pointer_axis_source(void *data, struct wl_pointer *pointer,
    uint32_t axis_source)
{
  (void)data;
  (void)pointer;
  (void)axis_source;
}

static void pointer_axis_stop(void *data, struct wl_pointer *pointer,
    uint32_t time, uint32_t axis)
{
  (void)data;
  (void)pointer;
  (void)time;
  (void)axis;
}

static void pointer_axis_discrete(void *data, struct wl_pointer *pointer,
    uint32_t axis, int32_t discrete)
{
  (void)data;
  (void)pointer;
  (void)axis;
  (void)discrete;
}

static const struct wl_pointer_listener pointer_listener = {
  .enter = pointer_enter,
  .leave = pointer_leave,
  .motion = pointer_motion,
  .button = pointer_button,
  .axis = pointer_axis,
  .frame = pointer_frame,
  .axis_source = pointer_axis_source,
  .axis_stop = pointer_axis_stop,
  .axis_discrete = pointer_axis_discrete
};

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities)
{
  (void)seat;
  struct imv_window *window = data;

  if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
    if (!window->wl_pointer) {
      window->wl_pointer = wl_seat_get_pointer(window->wl_seat);
      wl_pointer_add_listener(window->wl_pointer, &pointer_listener, window);
    }
  } else {
    if (window->wl_pointer) {
      wl_pointer_release(window->wl_pointer);
      window->wl_pointer = NULL;
    }
  }

  if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
    if (!window->wl_keyboard) {
      window->wl_keyboard = wl_seat_get_keyboard(window->wl_seat);
      wl_keyboard_add_listener(window->wl_keyboard, &keyboard_listener, window);
    }
  } else {
    if (window->wl_keyboard) {
      wl_keyboard_release(window->wl_keyboard);
      window->wl_keyboard = NULL;
    }
  }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
  (void)data;
  (void)seat;
  (void)name;
}

static const struct wl_seat_listener seat_listener = {
  .capabilities = seat_capabilities,
  .name = seat_name
};

static void on_global(void *data, struct wl_registry *registry, uint32_t id,
    const char *interface, uint32_t version)
{
  struct imv_window *window = data;

  if (!strcmp(interface, "wl_compositor")) {
    window->wl_compositor = 
      wl_registry_bind(registry, id, &wl_compositor_interface, version);
  } else if (!strcmp(interface, "xdg_wm_base")) {
    window->wl_xdg =
      wl_registry_bind(registry, id, &xdg_wm_base_interface, version);
    xdg_wm_base_add_listener(window->wl_xdg, &shell_listener_xdg, window);
  } else if (!strcmp(interface, "wl_seat")) {
    window->wl_seat = wl_registry_bind(registry, id, &wl_seat_interface, version);
    wl_seat_add_listener(window->wl_seat, &seat_listener, window);
  }
}

static void on_remove_global(void *data, struct wl_registry *registry, uint32_t id)
{
  (void)data;
  (void)registry;
  (void)id;
}

static const struct wl_registry_listener registry_listener = {
    .global = on_global,
    .global_remove = on_remove_global
};

static void surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
  struct imv_window *window = data;
  xdg_surface_ack_configure(surface, serial);
  wl_surface_commit(window->wl_surface);
}

static const struct xdg_surface_listener surface_listener = {
  .configure = surface_configure
};

static void toplevel_configure(void *data, struct xdg_toplevel *toplevel,
    int width, int height, struct wl_array *states)
{
  (void)toplevel;

  struct imv_window *window = data;
  window->width = width;
  window->height = height;
  window->fullscreen = false;

  enum xdg_toplevel_state *state;
  wl_array_for_each(state, states) {
    if (*state == XDG_TOPLEVEL_STATE_FULLSCREEN) {
      window->fullscreen = true;
    }
  }
  wl_egl_window_resize(window->egl_window, width, height, 0, 0);

  struct imv_event e = {
    .type = IMV_EVENT_RESIZE,
    .data = {
      .resize = {
        .width = width,
        .height = height,
        .buffer_width = width * window->scale,
        .buffer_height = height * window->scale
      }
    }
  };
  imv_window_push_event(window, &e);
}

static void toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
  (void)toplevel;

  struct imv_window *window = data;
  struct imv_event e = {
    .type = IMV_EVENT_CLOSE
  };
  imv_window_push_event(window, &e);
}

static const struct xdg_toplevel_listener toplevel_listener = {
  .configure = toplevel_configure,
  .close = toplevel_close
};

static void connect_to_wayland(struct imv_window *window)
{
  window->wl_display = wl_display_connect(NULL);
  assert(window->wl_display);
  window->display_fd = wl_display_get_fd(window->wl_display);
  window->event_fd = eventfd(0, 0);

  window->wl_registry = wl_display_get_registry(window->wl_display);
  assert(window->wl_registry);

  wl_registry_add_listener(window->wl_registry, &registry_listener, window);
  wl_display_dispatch(window->wl_display);
  assert(window->wl_compositor);
  assert(window->wl_xdg);
  assert(window->wl_seat);

  window->egl_display = eglGetDisplay(window->wl_display);
  eglInitialize(window->egl_display, NULL, NULL);
}

static void create_window(struct imv_window *window, int width, int height,
    const char *title)
{
  eglBindAPI(EGL_OPENGL_API);
  EGLint attributes[] = {
    EGL_RED_SIZE,   8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE,  8,
    EGL_NONE
  };
  EGLConfig config;
  EGLint num_config;
  eglChooseConfig(window->egl_display, attributes, &config, 1, &num_config);
  window->egl_context = eglCreateContext(window->egl_display, config, EGL_NO_CONTEXT, NULL);
  assert(window->egl_context);

  window->wl_surface = wl_compositor_create_surface(window->wl_compositor);
  assert(window->wl_surface);

  window->wl_xdg_surface = xdg_wm_base_get_xdg_surface(window->wl_xdg, window->wl_surface);
  assert(window->wl_xdg_surface);

  xdg_surface_add_listener(window->wl_xdg_surface, &surface_listener, window);

  window->wl_xdg_toplevel = xdg_surface_get_toplevel(window->wl_xdg_surface);
  assert(window->wl_xdg_toplevel);

  xdg_toplevel_add_listener(window->wl_xdg_toplevel, &toplevel_listener, window);

  wl_surface_commit(window->wl_surface);
  wl_display_roundtrip(window->wl_display);

  window->egl_window = wl_egl_window_create(window->wl_surface, width, height);
  window->egl_surface = eglCreateWindowSurface(window->egl_display, config, window->egl_window, NULL);
  eglMakeCurrent(window->egl_display, window->egl_surface, window->egl_surface, window->egl_context);

  wl_surface_commit(window->wl_surface);
  xdg_toplevel_set_title(window->wl_xdg_toplevel, title);
  wl_display_roundtrip(window->wl_display);
}

static void shutdown_wayland(struct imv_window *window)
{
  close(window->event_fd);
  if (window->wl_pointer) {
    wl_pointer_destroy(window->wl_pointer);
  }
  if (window->wl_keyboard) {
    wl_keyboard_destroy(window->wl_keyboard);
  }
  xdg_toplevel_destroy(window->wl_xdg_toplevel);
  xdg_surface_destroy(window->wl_xdg_surface);
  eglTerminate(window->egl_display);
  wl_surface_destroy(window->wl_surface);
}

struct imv_window *imv_window_create(int width, int height, const char *title)
{
  struct imv_window *window = calloc(1, sizeof *window);
  window->scale = 1;
  window->events.cap = 128;
  window->events.queue = malloc(window->events.cap * sizeof *window->events.queue);
  pthread_mutex_init(&window->events.mutex, NULL);
  connect_to_wayland(window);
  create_window(window, width, height, title);
  return window;
}

void imv_window_free(struct imv_window *window)
{
  pthread_mutex_destroy(&window->events.mutex);
  shutdown_wayland(window);
  free(window->events.queue);
  free(window);
}

void imv_window_get_size(struct imv_window *window, int *w, int *h)
{
  if (w) {
    *w = window->width;
  }
  if (h) {
    *h = window->height;
  }
}

void imv_window_get_framebuffer_size(struct imv_window *window, int *w, int *h)
{
  if (w) {
    *w = window->width * window->scale;
  }
  if (h) {
    *h = window->height * window->scale;
  }
}

void imv_window_set_title(struct imv_window *window, const char *title)
{
  xdg_toplevel_set_title(window->wl_xdg_toplevel, title);
}

bool imv_window_is_fullscreen(struct imv_window *window)
{
  return window->fullscreen;
}

void imv_window_set_fullscreen(struct imv_window *window, bool fullscreen)
{
  if (window->fullscreen && !fullscreen) {
    xdg_toplevel_unset_fullscreen(window->wl_xdg_toplevel);
  } else if (!window->fullscreen && fullscreen) {
    xdg_toplevel_set_fullscreen(window->wl_xdg_toplevel, NULL);
  }
}

bool imv_window_get_mouse_button(struct imv_window *window, int button)
{
  if (button == 1) {
    return window->pointer.mouse1.last;
  }
  return false;
}

void imv_window_get_mouse_position(struct imv_window *window, double *x, double *y)
{
  if (x) {
    *x = window->pointer.x.last;
  }
  if (y) {
    *y = window->pointer.y.last;
  }
}

void imv_window_present(struct imv_window *window)
{
  eglSwapBuffers(window->egl_display, window->egl_surface);
}

void imv_window_wait_for_event(struct imv_window *window, double timeout)
{
  struct pollfd fds[] = {
    {.fd = window->display_fd, .events = POLLIN},
    {.fd = window->event_fd,   .events = POLLIN}
  };
  nfds_t nfds = sizeof fds / sizeof *fds;

  while (wl_display_prepare_read(window->wl_display)) {
    wl_display_dispatch_pending(window->wl_display);
  }

  wl_display_flush(window->wl_display);

  int rc = poll(fds, nfds, timeout * 1000);
  if (rc < 0) {
    wl_display_cancel_read(window->wl_display);
    return;
  }

  /* Handle any new wayland events */
  if (fds[0].revents & POLLIN) {
    wl_display_read_events(window->wl_display);
    wl_display_dispatch_pending(window->wl_display);
  } else {
    wl_display_cancel_read(window->wl_display);
  }

  /* Clear the eventfd if hit */
  if (fds[1].revents & POLLIN) {
    uint64_t out;
    read(window->event_fd, &out, sizeof out);
  }
}

void imv_window_push_event(struct imv_window *window, struct imv_event *e)
{
  pthread_mutex_lock(&window->events.mutex);
  if (window->events.len == window->events.cap) {
    size_t new_cap = window->events.cap * 2;
    struct imv_event *new_queue =
      realloc(window->events.queue, new_cap * sizeof *window->events.queue);
    assert(new_queue);
    window->events.queue = new_queue;
  }

  window->events.queue[window->events.len++] = *e;
  pthread_mutex_unlock(&window->events.mutex);

  /* Wake up wait_events */
  uint64_t in = 1;
  write(window->event_fd, &in, sizeof in);
}

void imv_window_pump_events(struct imv_window *window, imv_event_handler handler, void *data)
{
  wl_display_dispatch_pending(window->wl_display);

  pthread_mutex_lock(&window->events.mutex);
  if (handler) {
    for (size_t i = 0; i < window->events.len; ++i) {
      handler(data, &window->events.queue[i]);
    }
  }
  window->events.len = 0;
  pthread_mutex_unlock(&window->events.mutex);
}
