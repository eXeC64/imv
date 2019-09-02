#include "window.h"

#include "keyboard.h"
#include "list.h"

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

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

  struct imv_keyboard *keyboard;
  struct list *wl_outputs;

  int display_fd;
  int pipe_fds[2];

  timer_t timer_id;
  int repeat_scancode; /* scancode of key to repeat */
  int repeat_delay; /* time before repeat in ms */
  int repeat_interval; /* time between repeats in ms */

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
};

struct output_data {
  struct wl_output *wl_output;
  int scale;
  int pending_scale;
  bool contains_window;
};

static void set_nonblocking(int fd)
{
  int flags = fcntl(fd, F_GETFL);
  assert(flags != -1);
  flags |= O_NONBLOCK;
  int rc = fcntl(fd, F_SETFL, flags);
  assert(rc != -1);
}

static void handle_ping_xdg_wm_base(void *data, struct xdg_wm_base *xdg,
    uint32_t serial)
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
  (void)keyboard;
  (void)format;
  struct imv_window *window = data;
  char *src = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
  imv_keyboard_set_keymap(window->keyboard, src);
  munmap(src, size);
  close(fd);
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

static void cleanup_event(struct imv_event *event)
{
  if (event->type == IMV_EVENT_KEYBOARD) {
    free(event->data.keyboard.keyname);
    free(event->data.keyboard.text);
  }
}

static void push_keypress(struct imv_window *window, int scancode)
{
  char keyname[32] = {0};
  imv_keyboard_keyname(window->keyboard, scancode, keyname, sizeof keyname);

  char text[64] = {0};
  imv_keyboard_get_text(window->keyboard, scancode, text, sizeof text);

  char *desc = imv_keyboard_describe_key(window->keyboard, scancode);
  if (!desc) {
    desc = strdup("");
  }

  struct imv_event e = {
    .type = IMV_EVENT_KEYBOARD,
    .data = {
      .keyboard = {
        .scancode = scancode,
        .keyname = strdup(keyname),
        .description = desc,
        .text = strdup(text),
      }
    }
  };
  imv_window_push_event(window, &e);
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard,
    uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
  (void)serial;
  (void)keyboard;
  (void)time;
  struct imv_window *window = data;

  imv_keyboard_update_key(window->keyboard, key, state);

  if (!state) {
    /* If a key repeat timer is running, stop it */
    struct itimerspec off = {
      .it_value = {
        .tv_sec = 0,
        .tv_nsec = 0,
      },
      .it_interval = {
        .tv_sec = 0,
        .tv_nsec = 0,
      },
    };
    timer_settime(window->timer_id, 0, &off, NULL);
    return;
  }

  push_keypress(window, key);

  if (imv_keyboard_should_key_repeat(window->keyboard, key)) {
    /* Kick off the key-repeat timer for the current key */
    window->repeat_scancode = key;
    struct itimerspec period = {
      .it_value = {
        .tv_sec = 0,
        .tv_nsec = window->repeat_delay * 1000 * 1000,
      },
      .it_interval = {
        .tv_sec = 0,
        .tv_nsec = window->repeat_interval * 1000 * 1000,
      },
    };
    timer_settime(window->timer_id, 0, &period, NULL);
  }
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
    uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
    uint32_t mods_locked, uint32_t group)
{
  (void)keyboard;
  (void)serial;
  (void)group;
  struct imv_window *window = data;

  imv_keyboard_update_mods(window->keyboard,
      mods_depressed,
      mods_latched,
      mods_locked);
}

static void keyboard_repeat(void *data, struct wl_keyboard *keyboard,
    int32_t rate, int32_t delay)
{
  (void)keyboard;
  struct imv_window *window = data;
  window->repeat_delay = delay;
  window->repeat_interval = 1000 / rate;
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

static void output_geometry(void *data, struct wl_output *wl_output, int32_t x,
    int32_t y, int32_t physical_width, int32_t physical_height,
    int32_t subpixel, const char *make, const char *model, int32_t transform)
{
  (void)data;
  (void)wl_output;
  (void)x;
  (void)y;
  (void)physical_width;
  (void)physical_height;
  (void)subpixel;
  (void)make;
  (void)model;
  (void)transform;
}

static void output_mode(void *data, struct wl_output *wl_output, uint32_t flags,
    int32_t width, int32_t height, int32_t refresh)
{
  (void)data;
  (void)wl_output;
  (void)flags;
  (void)width;
  (void)height;
  (void)refresh;
}

static void output_done(void *data, struct wl_output *wl_output)
{
  (void)data;
  struct output_data *output_data = wl_output_get_user_data(wl_output);
  output_data->scale = output_data->pending_scale;
}

static void output_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
  (void)data;
  struct output_data *output_data = wl_output_get_user_data(wl_output);
  output_data->pending_scale = factor;
}

static const struct wl_output_listener output_listener = {
  .geometry = output_geometry,
  .mode = output_mode,
  .done = output_done,
  .scale = output_scale
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
  } else if (!strcmp(interface, "wl_output")) {
    struct output_data *output_data = calloc(1, sizeof *output_data);
    output_data->wl_output = wl_registry_bind(registry, id, &wl_output_interface, version);
    output_data->pending_scale = 1;
    wl_output_set_user_data(output_data->wl_output, output_data);
    wl_output_add_listener(output_data->wl_output, &output_listener, output_data);
    list_append(window->wl_outputs, output_data);
  }
}

static void on_remove_global(void *data, struct wl_registry *registry, uint32_t id)
{
  (void)data;
  (void)registry;
  (void)id;
}

static void update_scale(struct imv_window *window)
{
  int new_scale = 1;
  for (size_t i = 0; i < window->wl_outputs->len; ++i) {
    struct output_data *data = window->wl_outputs->items[i];
    if (data->contains_window && data->scale > new_scale) {
      new_scale = data->scale;
    }
  }

  if (new_scale != window->scale) {
    window->scale = new_scale;
    wl_surface_set_buffer_scale(window->wl_surface, window->scale);
    wl_surface_commit(window->wl_surface);
    size_t buffer_width = window->width * window->scale;
    size_t buffer_height = window->height * window->scale;
    wl_egl_window_resize(window->egl_window, buffer_width, buffer_height, 0, 0);
    glViewport(0, 0, buffer_width, buffer_height);

    struct imv_event e = {
      .type = IMV_EVENT_RESIZE,
      .data = {
        .resize = {
          .width = window->width,
          .height = window->height,
          .buffer_width = buffer_width,
          .buffer_height = buffer_height
        }
      }
    };
    imv_window_push_event(window, &e);
  }
}

static const struct wl_registry_listener registry_listener = {
    .global = on_global,
    .global_remove = on_remove_global
};

static void surface_enter(void *data, struct wl_surface *wl_surface,
    struct wl_output *output)
{
  (void)wl_surface;
  struct output_data *output_data = wl_output_get_user_data(output);
  output_data->contains_window = true;
  update_scale(data);
}

static void surface_leave(void *data, struct wl_surface *wl_surface,
    struct wl_output *output)
{
  (void)wl_surface;
  struct output_data *output_data = wl_output_get_user_data(output);
  output_data->contains_window = false;
  update_scale(data);
}

static const struct wl_surface_listener surface_listener = {
  .enter = surface_enter,
  .leave = surface_leave
};

static void surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
  struct imv_window *window = data;
  xdg_surface_ack_configure(surface, serial);
  wl_surface_commit(window->wl_surface);
}

static const struct xdg_surface_listener shell_surface_listener = {
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
  size_t buffer_width = window->width * window->scale;
  size_t buffer_height = window->height * window->scale;
  wl_egl_window_resize(window->egl_window, buffer_width, buffer_height, 0, 0);
  glViewport(0, 0, buffer_width, buffer_height);

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
  pipe(window->pipe_fds);
  set_nonblocking(window->pipe_fds[0]);
  set_nonblocking(window->pipe_fds[1]);

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
  wl_surface_add_listener(window->wl_surface, &surface_listener, window);

  window->wl_xdg_surface = xdg_wm_base_get_xdg_surface(window->wl_xdg, window->wl_surface);
  assert(window->wl_xdg_surface);

  xdg_surface_add_listener(window->wl_xdg_surface, &shell_surface_listener, window);

  window->wl_xdg_toplevel = xdg_surface_get_toplevel(window->wl_xdg_surface);
  assert(window->wl_xdg_toplevel);

  xdg_toplevel_add_listener(window->wl_xdg_toplevel, &toplevel_listener, window);
  xdg_toplevel_set_title(window->wl_xdg_toplevel, title);
  xdg_toplevel_set_app_id(window->wl_xdg_toplevel, "imv");

  window->egl_window = wl_egl_window_create(window->wl_surface, width, height);
  window->egl_surface = eglCreateWindowSurface(window->egl_display, config, window->egl_window, NULL);
  eglMakeCurrent(window->egl_display, window->egl_surface, window->egl_surface, window->egl_context);

  wl_surface_commit(window->wl_surface);
  wl_display_roundtrip(window->wl_display);
}

static void shutdown_wayland(struct imv_window *window)
{
  close(window->pipe_fds[0]);
  close(window->pipe_fds[1]);
  if (window->wl_pointer) {
    wl_pointer_destroy(window->wl_pointer);
  }
  if (window->wl_keyboard) {
    wl_keyboard_destroy(window->wl_keyboard);
  }
  if (window->wl_seat) {
    wl_seat_destroy(window->wl_seat);
  }
  if (window->wl_xdg_toplevel) {
    xdg_toplevel_destroy(window->wl_xdg_toplevel);
  }
  if (window->wl_xdg_surface) {
    xdg_surface_destroy(window->wl_xdg_surface);
  }
  if (window->wl_xdg) {
    xdg_wm_base_destroy(window->wl_xdg);
  }
  if (window->egl_window) {
    wl_egl_window_destroy(window->egl_window);
  }
  eglTerminate(window->egl_display);
  if (window->wl_surface) {
    wl_surface_destroy(window->wl_surface);
  }
  if (window->wl_compositor) {
    wl_compositor_destroy(window->wl_compositor);
  }
  if (window->wl_registry) {
    wl_registry_destroy(window->wl_registry);
  }
  if (window->wl_display) {
    wl_display_disconnect(window->wl_display);
  }
}

static void on_timer(union sigval sigval)
{
  struct imv_window *window = sigval.sival_ptr;
  push_keypress(window, window->repeat_scancode);
}

struct imv_window *imv_window_create(int width, int height, const char *title)
{
  /* Ensure event writes will always be atomic */
  assert(sizeof(struct imv_event) <= PIPE_BUF);

  struct imv_window *window = calloc(1, sizeof *window);
  window->scale = 1;

  window->keyboard = imv_keyboard_create();
  assert(window->keyboard);
  window->wl_outputs = list_create();
  connect_to_wayland(window);
  create_window(window, width, height, title);

  struct sigevent timer_handler = {
    .sigev_notify = SIGEV_THREAD,
    .sigev_value.sival_ptr = window,
    .sigev_notify_function = on_timer,
  };
  int timer_rc = timer_create(CLOCK_MONOTONIC, &timer_handler, &window->timer_id);
  assert(timer_rc == 0);
  return window;
}

void imv_window_free(struct imv_window *window)
{
  timer_delete(window->timer_id);
  imv_keyboard_free(window->keyboard);
  shutdown_wayland(window);
  list_deep_free(window->wl_outputs);
  free(window);
}

void imv_window_clear(struct imv_window *window, unsigned char r,
    unsigned char g, unsigned char b)
{
  (void)window;
  glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
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
    {.fd = window->display_fd,  .events = POLLIN},
    {.fd = window->pipe_fds[0], .events = POLLIN}
  };
  nfds_t nfds = sizeof fds / sizeof *fds;

  if (wl_display_prepare_read(window->wl_display)) {
    /* If an event's already in the wayland queue we return */
    return;
  }

  wl_display_flush(window->wl_display);

  if (poll(fds, nfds, timeout * 1000) <= 0) {
    wl_display_cancel_read(window->wl_display);
    return;
  }

  if (fds[0].revents & POLLIN) {
    wl_display_read_events(window->wl_display);
  } else {
    wl_display_cancel_read(window->wl_display);
  }
}

void imv_window_push_event(struct imv_window *window, struct imv_event *e)
{
  /* Push it down the pipe */
  write(window->pipe_fds[1], e, sizeof *e);
}

void imv_window_pump_events(struct imv_window *window, imv_event_handler handler, void *data)
{
  wl_display_dispatch_pending(window->wl_display);

  while (1) {
    struct imv_event e;
    ssize_t len = read(window->pipe_fds[0], &e, sizeof e);
    if (len <= 0) {
      break;
    }
    assert(len == sizeof e);
    if (handler) {
      handler(data, &e);
    }
    cleanup_event(&e);
  }
}
