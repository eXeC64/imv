#include "window.h"

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <assert.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>

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
};

struct imv_window *imv_window_create(int w, int h, const char *title)
{
  struct imv_window *window = calloc(1, sizeof *window);
  window->pointer.last.x = -1;
  window->pointer.last.y = -1;

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

  XMapWindow(window->x_display, window->x_window);
  XStoreName(window->x_display, window->x_window, title);

  window->x_glc = glXCreateContext(window->x_display, vi, NULL, GL_TRUE);
  assert(window->x_glc);
  glXMakeCurrent(window->x_display, window->x_window, window->x_glc);

  return window;
}

void imv_window_free(struct imv_window *window)
{
  glXDestroyContext(window->x_display, window->x_glc);
  XCloseDisplay(window->x_display);
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
  };
  nfds_t nfds = sizeof fds / sizeof *fds;

  poll(fds, nfds, timeout * 1000);
}

void imv_window_push_event(struct imv_window *window, struct imv_event *e)
{
  XEvent xe = {
    .xclient = {
      .type = ClientMessage,
      .format = 8
    }
  };
  memcpy(&xe.xclient.data.l[0], &e->data.custom, sizeof(void*));
  XSendEvent(window->x_display, window->x_window, True, 0xfff, &xe);
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
      struct imv_event e = {
        .type = IMV_EVENT_KEYBOARD,
        .data = {
          .keyboard = {
            .scancode = xev.xkey.keycode - 8,
            .pressed = xev.type == KeyPress
          }
        }
      };
      if (handler) {
        handler(data, &e);
      }
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
    } else if (xev.type == ClientMessage) {
      struct imv_event e = {
        .type = IMV_EVENT_CUSTOM,
        .data = {
          .custom = (void*)*((void**)&xev.xclient.data.l[0])
        }
      };
      if (handler) {
        handler(data, &e);
      }
    }
  }

}

