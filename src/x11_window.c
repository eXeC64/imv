#include "window.h"

#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <assert.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>

struct imv_window {
  Display *x_display;
  Window x_window;
  GLXContext x_glc;
};

struct imv_window *imv_window_create(int w, int h, const char *title)
{
  struct imv_window *window = calloc(1, sizeof *window);
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
  };

  window->x_window = XCreateWindow(window->x_display, root, 0, 0, w, h,
      0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &wa);

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
  XStoreName(window->x_display, window->x_window, title);
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

