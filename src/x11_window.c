#include "window.h"

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon-x11.h>

#include "keyboard.h"
#include "log.h"

struct imv_window {
  Display    *x_display;
  Window     x_window;
  GLXContext x_glc;
  Atom       x_state;
  Atom       x_fullscreen;
  int width;
  int height;
  struct {
    struct {
      int x, y;
    } last;
    struct {
      int x, y;
    } current;
    bool mouse1;
  } pointer;

  struct imv_keyboard *keyboard;
  int pipe_fds[2];
  char *keymap;
};

static void set_nonblocking(int fd)
{
  int flags = fcntl(fd, F_GETFL);
  assert(flags != -1);
  flags |= O_NONBLOCK;
  int rc = fcntl(fd, F_SETFL, flags);
  assert(rc != -1);
}

static void setup_keymap(struct imv_window *window)
{
  xcb_connection_t *conn = xcb_connect(NULL, NULL);
  if (xcb_connection_has_error(conn)) {
    imv_log(IMV_ERROR, "x11_window: Failed to load keymap. Could not connect via xcb.");
    return;
  }

  if (!xkb_x11_setup_xkb_extension(conn,
        XKB_X11_MIN_MAJOR_XKB_VERSION,
        XKB_X11_MIN_MINOR_XKB_VERSION,
        0, NULL, NULL, NULL, NULL)) {
    xcb_disconnect(conn);
    imv_log(IMV_ERROR, "x11_window: Failed to load keymap. xkb extension not supported by server.");
    return;
  }

  int32_t device = xkb_x11_get_core_keyboard_device_id(conn);

  struct xkb_context *context = xkb_context_new(0);
  if (!context) {
    xcb_disconnect(conn);
    imv_log(IMV_ERROR, "x11_window: Failed to load keymap. Failed to initialise xkb context.");
    return;
  }

  struct xkb_keymap *keymap =
    xkb_x11_keymap_new_from_device(context, conn, device, 0);
  if (keymap) {
    window->keymap = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_USE_ORIGINAL_FORMAT);
  } else {
    imv_log(IMV_ERROR, "x11_window: Failed to load keymap. xkb_x11_keymap_new_from_device returned NULL.");
  }
  xkb_context_unref(context);

  xcb_disconnect(conn);
}

struct imv_window *imv_window_create(int w, int h, const char *title)
{
  /* Ensure event writes will always be atomic */
  assert(sizeof(struct imv_event) <= PIPE_BUF);

  struct imv_window *window = calloc(1, sizeof *window);
  window->pointer.last.x = -1;
  window->pointer.last.y = -1;
  pipe(window->pipe_fds);
  set_nonblocking(window->pipe_fds[0]);
  set_nonblocking(window->pipe_fds[1]);

  window->x_display = XOpenDisplay(NULL);
  assert(window->x_display);
  Window root = DefaultRootWindow(window->x_display);
  assert(root);
  
  GLint att[] = {
    GLX_RGBA,
    GLX_DEPTH_SIZE,
    24,
    GLX_DOUBLEBUFFER,
    None
  };

  XVisualInfo *vi = glXChooseVisual(window->x_display, 0, att);
  assert(vi);

  Colormap cmap = XCreateColormap(window->x_display, root,
      vi->visual, AllocNone);

  XSetWindowAttributes wa = {
    .colormap = cmap,
    .event_mask = ExposureMask | KeyPressMask | KeyReleaseMask
                | ButtonPressMask | ButtonReleaseMask | PointerMotionMask
  };

  window->x_window = XCreateWindow(window->x_display, root, 0, 0, w, h,
      0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &wa);

  window->x_state = XInternAtom(window->x_display, "_NET_WM_STATE", true);
  window->x_fullscreen = XInternAtom(window->x_display, "_NET_WM_STATE_FULLSCREEN", true);

  XClassHint hint = {
    .res_name = "imv",
    .res_class= "imv",
  };
  XSetClassHint(window->x_display, window->x_window, &hint);
  XMapWindow(window->x_display, window->x_window);
  XStoreName(window->x_display, window->x_window, title);

  window->x_glc = glXCreateContext(window->x_display, vi, NULL, GL_TRUE);
  assert(window->x_glc);
  glXMakeCurrent(window->x_display, window->x_window, window->x_glc);

  window->keyboard = imv_keyboard_create();
  assert(window->keyboard);

  setup_keymap(window);
  imv_keyboard_set_keymap(window->keyboard, window->keymap);

  return window;
}

void imv_window_free(struct imv_window *window)
{
  imv_keyboard_free(window->keyboard);
  close(window->pipe_fds[0]);
  close(window->pipe_fds[1]);
  glXDestroyContext(window->x_display, window->x_glc);
  XCloseDisplay(window->x_display);
  free(window->keymap);
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
    *w = window->width;
  }
  if (h) {
    *h = window->height;
  }
}

void imv_window_set_title(struct imv_window *window, const char *title)
{
  XStoreName(window->x_display, window->x_window, title);
}

bool imv_window_is_fullscreen(struct imv_window *window)
{
  size_t count = 0;
  Atom type;
  int format;
  size_t after;
  Atom *props = NULL;
  XGetWindowProperty(window->x_display, window->x_window, window->x_state,
      0, 1024, False, XA_ATOM, &type, &format, &count, &after, (unsigned char**)&props);

  bool fullscreen = false;
  for (size_t i = 0; i < count; ++i) {
    if (props[i] == window->x_fullscreen) {
      fullscreen = true;
      break;
    }
  }
  XFree(props);
  return fullscreen;
}

void imv_window_set_fullscreen(struct imv_window *window, bool fullscreen)
{
  Window root = DefaultRootWindow(window->x_display);
  XEvent event = {
    .xclient = {
      .type = ClientMessage,
      .window = window->x_window,
      .format = 32,
      .message_type = window->x_state,
      .data = {
        .l = {
          (fullscreen ? 1 : 0),
          window->x_fullscreen,
          0,
          1
        }
      }
    }
  };
  XSendEvent(window->x_display, root, False,
      SubstructureNotifyMask | SubstructureRedirectMask, &event );
}

bool imv_window_get_mouse_button(struct imv_window *window, int button)
{
  if (button == 1) {
    return window->pointer.mouse1;
  }
  return false;
}

void imv_window_get_mouse_position(struct imv_window *window, double *x, double *y)
{
  if (x) {
    *x = window->pointer.current.x;
  }
  if (y) {
    *y = window->pointer.current.y;
  }
}

void imv_window_present(struct imv_window *window)
{
  glXSwapBuffers(window->x_display, window->x_window);
}

void imv_window_wait_for_event(struct imv_window *window, double timeout)
{
  struct pollfd fds[] = {
    {.fd = ConnectionNumber(window->x_display), .events = POLLIN},
    {.fd = window->pipe_fds[0], .events = POLLIN}
  };
  nfds_t nfds = sizeof fds / sizeof *fds;

  poll(fds, nfds, timeout * 1000);
}

void imv_window_push_event(struct imv_window *window, struct imv_event *e)
{
  /* Push it down the pipe */
  write(window->pipe_fds[1], e, sizeof *e);
}

static void handle_keyboard(struct imv_window *window, imv_event_handler handler, void *data, const XEvent *xev)
{
  imv_keyboard_update_mods(window->keyboard, (int)xev->xkey.state, 0, 0);

  bool pressed = xev->type == KeyPress;
  int scancode = xev->xkey.keycode - 8;
  imv_keyboard_update_key(window->keyboard, scancode, pressed);

  if (!pressed) {
    return;
  }

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
        .keyname = keyname,
        .description = desc,
        .text = text,
      }
    }
  };

  if (handler) {
    handler(data, &e);
  }
  free(desc);
}

void imv_window_pump_events(struct imv_window *window, imv_event_handler handler, void *data)
{
  XEvent xev;

  while (XPending(window->x_display)) {
    XNextEvent(window->x_display, &xev);

    if (xev.type == Expose) {
      XWindowAttributes wa;
      XGetWindowAttributes(window->x_display, window->x_window, &wa);
      window->width = wa.width;
      window->height = wa.height;
      glViewport(0, 0, wa.width, wa.height);
      struct imv_event e = {
        .type = IMV_EVENT_RESIZE,
        .data = {
          .resize = {
            .width = wa.width,
            .height = wa.height,
            .buffer_width = wa.width,
            .buffer_height = wa.height
          }
        }
      };
      if (handler) {
        handler(data, &e);
      }
    } else if (xev.type == KeyPress || xev.type == KeyRelease) {
      handle_keyboard(window, handler, data, &xev);
    } else if (xev.type == ButtonPress || xev.type == ButtonRelease) {
      if (xev.xbutton.button == Button1) {
        window->pointer.mouse1 = xev.type == ButtonPress;
        struct imv_event e = {
          .type = IMV_EVENT_MOUSE_BUTTON,
          .data = {
            .mouse_button = {
              .button = 1,
              .pressed = xev.type == ButtonPress
            }
          }
        };
        if (handler) {
          handler(data, &e);
        }
      } else if (xev.xbutton.button == Button4 || xev.xbutton.button == Button5) {
        struct imv_event e = {
          .type = IMV_EVENT_MOUSE_SCROLL,
          .data = {
            .mouse_scroll = {
              .dx = 0,
              .dy = xev.xbutton.button == Button4 ? -1 : 1
            }
          }
        };
        if (handler) {
          handler(data, &e);
        }
      }
    } else if (xev.type == MotionNotify) {
      window->pointer.current.x = xev.xmotion.x;
      window->pointer.current.y = xev.xmotion.y;
      int dx = window->pointer.current.x - window->pointer.last.x;
      int dy = window->pointer.current.y - window->pointer.last.y;
      if (window->pointer.last.x == -1) {
        dx = 0;
      }
      if (window->pointer.last.y == -1) {
        dy = 0;
      }
      window->pointer.last.x = window->pointer.current.x;
      window->pointer.last.y = window->pointer.current.y;

      struct imv_event e = {
        .type = IMV_EVENT_MOUSE_MOTION,
        .data = {
          .mouse_motion = {
            .x = window->pointer.current.x,
            .y = window->pointer.current.y,
            .dx = dx,
            .dy = dy
          }
        }
      };
      if (handler) {
        handler(data, &e);
      }
    }
  }

  /* Handle any events in the pipe */
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
  }
}

const char *imv_window_get_keymap(struct imv_window *window)
{
  return window->keymap;
}
