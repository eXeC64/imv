#include "imv.h"

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <wordexp.h>

#include "backend.h"
#include "binds.h"
#include "canvas.h"
#include "commands.h"
#include "console.h"
#include "image.h"
#include "ini.h"
#include "ipc.h"
#include "keyboard.h"
#include "list.h"
#include "log.h"
#include "navigator.h"
#include "source.h"
#include "viewport.h"
#include "window.h"

/* Some systems like GNU/Hurd don't define PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

enum scaling_mode {
  SCALING_NONE,
  SCALING_DOWN,
  SCALING_FULL,
  SCALING_MODE_COUNT
};

static const char *scaling_label[] = {
  "actual size",
  "shrink to fit",
  "scale to fit"
};

enum background_type {
  BACKGROUND_SOLID,
  BACKGROUND_CHEQUERED,
  BACKGROUND_TYPE_COUNT
};

struct backend_chain {
  const struct imv_backend *backend;
  struct backend_chain *next;
};

enum internal_event_type {
  NEW_IMAGE,
  BAD_IMAGE,
  NEW_PATH,
  COMMAND
};

struct internal_event {
  enum internal_event_type type;
  union {
    struct {
      struct imv_image *image;
      int frametime;
      bool is_new_image;
    } new_image;
    struct {
      char *error;
    } bad_image;
    struct {
      char *path;
    } new_path;
    struct {
      char *text;
    } command;
  } data;
};

struct imv {
  /* set to true to trigger clean exit */
  bool quit;

  /* indicates a new image is being loaded */
  bool loading;

  /* initial fullscreen state */
  bool start_fullscreen;

  /* initial window dimensions */
  int initial_width;
  int initial_height;

  /* display some textual info onscreen */
  bool overlay_enabled;

  /* method for scaling up images: interpolate or nearest neighbour */
  enum upscaling_method upscaling_method;

  /* dirty state flags */
  bool need_redraw;
  bool need_rescale;

  /* traverse sub-directories for more images */
  bool recursive_load;

  /* 'next' on the last image goes back to the first */
  bool loop_input;

  /* print all paths to stdout on clean exit */
  bool list_files_at_exit;

  /* read paths from stdin, as opposed to image data */
  bool paths_from_stdin;

  /* scale up / down images to match window, or actual size */
  enum scaling_mode scaling_mode;

  struct {
    /* show a solid background colour, or chequerboard pattern */
    enum background_type type;
    /* the aforementioned background colour */
    struct { unsigned char r, g, b; } color;
  } background;

  /* slideshow state tracking */
  struct {
    double duration;
    double elapsed;
  } slideshow;

  struct {
    /* for animated images, the getTime() time to display the next frame */
    double due;
    /* how long the next frame to be put onscreen should be displayed for */
    double duration;
    /* the next frame of an animated image, pre-fetched */
    struct imv_image *image;
    /* force the next frame to load, even if early */
    bool force_next_frame;
  } next_frame;

  struct imv_image *current_image;

  /* overlay font */
  struct {
    char *name;
    int size;
  } font;

  /* if specified by user, the path of the first image to display */
  char *starting_path;

  /* the user-specified format strings for the overlay and window title */
  char *title_text;
  char *overlay_text;

  /* imv subsystems */
  struct imv_binds *binds;
  struct imv_navigator *navigator;
  struct backend_chain *backends;
  struct imv_source *current_source;
  struct imv_source *last_source;
  struct imv_commands *commands;
  struct imv_console *console;
  struct imv_ipc *ipc;
  struct imv_viewport *view;
  struct imv_keyboard *keyboard;
  struct imv_canvas *canvas;
  struct imv_window *window;

  /* if reading an image from stdin, this is the buffer for it */
  void *stdin_image_data;
  size_t stdin_image_data_len;
};

static void command_quit(struct list *args, const char *argstr, void *data);
static void command_pan(struct list *args, const char *argstr, void *data);
static void command_next(struct list *args, const char *argstr, void *data);
static void command_prev(struct list *args, const char *argstr, void *data);
static void command_goto(struct list *args, const char *argstr, void *data);
static void command_zoom(struct list *args, const char *argstr, void *data);
static void command_open(struct list *args, const char *argstr, void *data);
static void command_close(struct list *args, const char *argstr, void *data);
static void command_fullscreen(struct list *args, const char *argstr, void *data);
static void command_overlay(struct list *args, const char *argstr, void *data);
static void command_exec(struct list *args, const char *argstr, void *data);
static void command_center(struct list *args, const char *argstr, void *data);
static void command_reset(struct list *args, const char *argstr, void *data);
static void command_next_frame(struct list *args, const char *argstr, void *data);
static void command_toggle_playing(struct list *args, const char *argstr, void *data);
static void command_set_scaling_mode(struct list *args, const char *argstr, void *data);
static void command_set_slideshow_duration(struct list *args, const char *argstr, void *data);
static void command_set_background(struct list *args, const char *argstr, void *data);

static bool setup_window(struct imv *imv);
static void consume_internal_event(struct imv *imv, struct internal_event *event);
static void render_window(struct imv *imv);
static void update_env_vars(struct imv *imv);
static size_t generate_env_text(struct imv *imv, char *buf, size_t len, const char *format);
static size_t read_from_stdin(void **buffer);

/* Finds the next split between commands in a string (';'). Provides a pointer
 * to the next character after the delimiter as out, or a pointer to '\0' if
 * nothing is left. Also provides the len from start up to the delimiter.
 */
static void split_commands(const char *start, const char **out, size_t *len)
{
  bool in_single_quotes = false;
  bool in_double_quotes = false;

  const char *str = start;

  while (*str) {
    if (!in_single_quotes && *str == '"') {
      in_double_quotes = !in_double_quotes;
    } else if (!in_double_quotes && *str == '\'') {
      in_single_quotes = !in_single_quotes;
    } else if (*str == '\\') {
      /* We don't care about the behaviour of any escaped character, just
       * make sure to skip over them. We do need to make sure not to allow
       * escaping of the null terminator though.
       */
      if (str[1] != '\0') {
        ++str;
      }
    } else if (!in_single_quotes && !in_double_quotes && *str == ';') {
      /* Found a command split that wasn't escaped or quoted */
      *len = str - start;
      *out = str + 1;
      return;
    }
    ++str;
  }

  *out = str;
  *len = str - start;
}

static bool add_bind(struct imv *imv, const char *keys, const char *commands)
{
  struct list *list = imv_bind_parse_keys(keys);
  if (!list) {
    imv_log(IMV_ERROR, "Invalid key combination");
    return false;
  }

  char command_buf[512];
  const char *next_command;
  size_t command_len;

  bool success = true;

  imv_binds_clear_key(imv->binds, list);
  while (*commands != '\0') {
    split_commands(commands, &next_command, &command_len);

    if (command_len >= sizeof command_buf) {
      imv_log(IMV_ERROR, "Command exceeded max length, not binding: %.*s\n", (int)command_len, commands);
      imv_binds_clear_key(imv->binds, list);
      success = false;
      break;
    }
    strncpy(command_buf, commands, command_len);
    command_buf[command_len] = '\0';

    enum bind_result result = imv_binds_add(imv->binds, list, command_buf);

    if (result == BIND_INVALID_KEYS) {
      imv_log(IMV_ERROR, "Invalid keys to bind to");
      success = false;
      break;
    } else if (result == BIND_INVALID_COMMAND) {
      imv_log(IMV_ERROR, "No command given to bind to");
      success = false;
      break;
    } else if (result == BIND_CONFLICTS) {
      imv_log(IMV_ERROR, "Key combination conflicts with existing bind");
      success = false;
      break;
    }
    commands = next_command;
  }

  list_deep_free(list);

  return success;
}

static double cur_time(void)
{
  struct timespec ts;
  const int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
  assert(!rc);
  return ts.tv_sec + (double)ts.tv_nsec * 0.000000001;
}

static void *async_free_source_thread(void *raw)
{
  struct imv_source *src = raw;
  src->free(src);
  return NULL;
}

static void async_free_source(struct imv_source *src)
{
  typedef void *(*thread_func)(void*);
  pthread_t thread;
  pthread_create(&thread, NULL, (thread_func)async_free_source_thread, src);
  pthread_detach(thread);
}

static void async_load_first_frame(struct imv_source *src)
{
  typedef void *(*thread_func)(void*);
  pthread_t thread;
  pthread_create(&thread, NULL, (thread_func)src->load_first_frame, src);
  pthread_detach(thread);
}

static void async_load_next_frame(struct imv_source *src)
{
  typedef void *(*thread_func)(void*);
  pthread_t thread;
  pthread_create(&thread, NULL, (thread_func)src->load_next_frame, src);
  pthread_detach(thread);
}

static void source_callback(struct imv_source_message *msg)
{
  struct imv *imv = msg->user_data;
  if (msg->source != imv->current_source) {
    /* We received a message from an old source, tidy up contents
     * as required, but ignore it.
     */
    if (msg->image) {
      imv_image_free(msg->image);
    }
    return;
  }

  struct internal_event *event = calloc(1, sizeof *event);
  if (msg->image) {
    event->type = NEW_IMAGE;
    event->data.new_image.image = msg->image;
    event->data.new_image.frametime = msg->frametime;

    /* Keep track of the last source to send us an image in order to detect
     * when we're getting a new image, as opposed to a new frame from the
     * same image.
     */
    event->data.new_image.is_new_image = msg->source != imv->last_source;
    imv->last_source = msg->source;
  } else {
    event->type = BAD_IMAGE;
    /* TODO: Something more elegant with error messages */
    event->data.bad_image.error = strdup(msg->error);
  }

  struct imv_event e = {
    .type = IMV_EVENT_CUSTOM,
    .data = {
      .custom = event
    }
  };

  imv_window_push_event(imv->window, &e);
}

static void command_callback(const char *text, void *data)
{
  struct imv *imv = data;

  struct internal_event *event = calloc(1, sizeof *event);
  event->type = COMMAND;
  event->data.command.text = strdup(text);

  struct imv_event e = {
    .type = IMV_EVENT_CUSTOM,
    .data = {
      .custom = event
    }
  };
  imv_window_push_event(imv->window, &e);
}

static void key_handler(struct imv *imv, int scancode, bool pressed)
{
  imv_keyboard_update_key(imv->keyboard, scancode, pressed);

  if (!pressed) {
    return;
  }

  char keyname[128] = {0};
  imv_keyboard_keyname(imv->keyboard, scancode, keyname, sizeof keyname);

  if (imv_console_is_active(imv->console)) {

    if (imv_console_key(imv->console, keyname)) {
      imv->need_redraw = true;
      return;
    }

    char text[128];
    size_t len = imv_keyboard_get_text(imv->keyboard, scancode, text, sizeof text);
    if (len >= sizeof text) {
      imv_log(IMV_WARNING, "Keyboard input too large for input buffer. Discarding.\n");
    } else {
      imv_console_input(imv->console, text);
    }

  } else {
    /* In regular mode see if we should enter command mode, otherwise send input
     * to the bind system.
     */

    if (!strcmp("colon", keyname)) {
      imv_console_activate(imv->console);
      imv->need_redraw = true;
      return;
    }

    char *keyname = imv_keyboard_describe_key(imv->keyboard, scancode);
    if (!keyname) {
      return;
    }

    struct list *cmds = imv_bind_handle_event(imv->binds, keyname);
    if (cmds) {
      imv_command_exec_list(imv->commands, cmds, imv);
    }
    free(keyname);
  }

  imv->need_redraw = true;
}


static void event_handler(void *data, const struct imv_event *e)
{
  struct imv *imv = data;
  switch (e->type) {
    case IMV_EVENT_CLOSE:
      imv->quit = true;
      break;
    case IMV_EVENT_RESIZE:
      {
        const int ww = e->data.resize.width;
        const int wh = e->data.resize.height;
        const int bw = e->data.resize.buffer_width;
        const int bh = e->data.resize.buffer_height;
        imv_viewport_update(imv->view, ww, wh, bw, bh, imv->current_image);
        imv_canvas_resize(imv->canvas, bw, bh);
        break;
      }
    case IMV_EVENT_KEYBOARD:
      key_handler(imv, e->data.keyboard.scancode, e->data.keyboard. pressed);
      break;
    case IMV_EVENT_MOUSE_MOTION:
      if (imv_window_get_mouse_button(imv->window, 1)) {
        imv_viewport_move(imv->view, e->data.mouse_motion.dx,
            e->data.mouse_motion.dy, imv->current_image);
      }
      break;
    case IMV_EVENT_MOUSE_SCROLL:
      {
        double x, y;
        imv_window_get_mouse_position(imv->window, &x, &y);
        imv_viewport_zoom(imv->view, imv->current_image, IMV_ZOOM_MOUSE,
            x, y, -e->data.mouse_scroll.dy);
      }
      break;
    case IMV_EVENT_CUSTOM:
      consume_internal_event(imv, e->data.custom);
      break;
    default:
      break;
  }

}

static void log_to_stderr(enum imv_log_level level, const char *text, void *data)
{
  (void)data;
  if (level >= IMV_INFO) {
    fprintf(stderr, "%s", text);
  }
}

struct imv *imv_create(void)
{
  /* Attach log to stderr */
  imv_log_add_log_callback(&log_to_stderr, NULL);

  struct imv *imv = calloc(1, sizeof *imv);
  imv->initial_width = 1280;
  imv->initial_height = 720;
  imv->need_redraw = true;
  imv->need_rescale = true;
  imv->scaling_mode = SCALING_FULL;
  imv->loop_input = true;
  imv->font.name = strdup("Monospace");
  imv->font.size = 24;
  imv->binds = imv_binds_create();
  imv->navigator = imv_navigator_create();
  imv->commands = imv_commands_create();
  imv->console = imv_console_create();
  imv_console_set_command_callback(imv->console, &command_callback, imv);
  imv->ipc = imv_ipc_create();
  imv_ipc_set_command_callback(imv->ipc, &command_callback, imv);
  imv->title_text = strdup(
      "imv - [${imv_current_index}/${imv_file_count}]"
      " [${imv_width}x${imv_height}] [${imv_scale}%]"
      " $imv_current_file [$imv_scaling_mode]"
  );
  imv->overlay_text = strdup(
      "[${imv_current_index}/${imv_file_count}]"
      " [${imv_width}x${imv_height}] [${imv_scale}%]"
      " $imv_current_file [$imv_scaling_mode]"
  );

  imv_command_register(imv->commands, "quit", &command_quit);
  imv_command_register(imv->commands, "pan", &command_pan);
  imv_command_register(imv->commands, "next", &command_next);
  imv_command_register(imv->commands, "prev", &command_prev);
  imv_command_register(imv->commands, "goto", &command_goto);
  imv_command_register(imv->commands, "zoom", &command_zoom);
  imv_command_register(imv->commands, "open", &command_open);
  imv_command_register(imv->commands, "close", &command_close);
  imv_command_register(imv->commands, "fullscreen", &command_fullscreen);
  imv_command_register(imv->commands, "overlay", &command_overlay);
  imv_command_register(imv->commands, "exec", &command_exec);
  imv_command_register(imv->commands, "center", &command_center);
  imv_command_register(imv->commands, "reset", &command_reset);
  imv_command_register(imv->commands, "next_frame", &command_next_frame);
  imv_command_register(imv->commands, "toggle_playing", &command_toggle_playing);
  imv_command_register(imv->commands, "scaling", &command_set_scaling_mode);
  imv_command_register(imv->commands, "slideshow", &command_set_slideshow_duration);
  imv_command_register(imv->commands, "background", &command_set_background);

  imv_command_alias(imv->commands, "q", "quit");
  imv_command_alias(imv->commands, "n", "next");
  imv_command_alias(imv->commands, "p", "prev");
  imv_command_alias(imv->commands, "g", "goto");
  imv_command_alias(imv->commands, "z", "zoom");
  imv_command_alias(imv->commands, "o", "open");
  imv_command_alias(imv->commands, "bg", "background");
  imv_command_alias(imv->commands, "ss", "slideshow");

  add_bind(imv, "q", "quit");
  add_bind(imv, "<Left>", "prev");
  add_bind(imv, "<bracketleft>", "prev");
  add_bind(imv, "<Right>", "next");
  add_bind(imv, "<bracketright>", "next");
  add_bind(imv, "gg", "goto 0");
  add_bind(imv, "<Shift+G>", "goto -1");
  add_bind(imv, "j", "pan 0 -50");
  add_bind(imv, "k", "pan 0 50");
  add_bind(imv, "h", "pan 50 0");
  add_bind(imv, "l", "pan -50 0");
  add_bind(imv, "x", "close");
  add_bind(imv, "f", "fullscreen");
  add_bind(imv, "d", "overlay");
  add_bind(imv, "p", "exec echo $imv_current_file");
  add_bind(imv, "<equal>", "zoom 1");
  add_bind(imv, "<Up>", "zoom 1");
  add_bind(imv, "<Shift+plus>", "zoom 1");
  add_bind(imv, "i", "zoom 1");
  add_bind(imv, "<Down>", "zoom -1");
  add_bind(imv, "<minus>", "zoom -1");
  add_bind(imv, "o", "zoom -1");
  add_bind(imv, "c", "center");
  add_bind(imv, "s", "scaling_mode next");
  add_bind(imv, "a", "zoom actual");
  add_bind(imv, "r", "reset");
  add_bind(imv, "<period>", "next_frame");
  add_bind(imv, "<space>", "toggle_playing");
  add_bind(imv, "t", "slideshow +1");
  add_bind(imv, "<Shift+T>", "slideshow -1");

  return imv;
}

void imv_free(struct imv *imv)
{
  free(imv->font.name);
  free(imv->title_text);
  free(imv->overlay_text);
  imv_binds_free(imv->binds);
  imv_navigator_free(imv->navigator);
  if (imv->current_source) {
    imv->current_source->free(imv->current_source);
  }
  imv_commands_free(imv->commands);
  imv_console_free(imv->console);
  imv_ipc_free(imv->ipc);
  imv_viewport_free(imv->view);
  imv_keyboard_free(imv->keyboard);
  imv_canvas_free(imv->canvas);
  if (imv->current_image) {
    imv_image_free(imv->current_image);
  }
  if (imv->next_frame.image) {
    imv_image_free(imv->next_frame.image);
  }
  if (imv->stdin_image_data) {
    free(imv->stdin_image_data);
  }
  if (imv->window) {
    imv_window_free(imv->window);
  }

  struct backend_chain *backend = imv->backends;
  while (backend) {
    struct backend_chain *next = backend->next;
    free(backend);
    backend = next;
  }

  free(imv);
}

void imv_install_backend(struct imv *imv, const struct imv_backend *backend)
{
  struct backend_chain *chain = malloc(sizeof *chain);
  chain->backend = backend;
  chain->next = imv->backends;
  imv->backends = chain;
}

static bool parse_bg(struct imv *imv, const char *bg)
{
  if (!strcmp("checks", bg)) {
    imv->background.type = BACKGROUND_CHEQUERED;
  } else {
    imv->background.type = BACKGROUND_SOLID;
    if (*bg == '#')
      ++bg;
    char *ep;
    uint32_t n = strtoul(bg, &ep, 16);
    if (*ep != '\0' || ep - bg != 6 || n > 0xFFFFFF) {
      imv_log(IMV_ERROR, "Invalid hex color: '%s'\n", bg);
      return false;
    }
    imv->background.color.b = n & 0xFF;
    imv->background.color.g = (n >> 8) & 0xFF;
    imv->background.color.r = (n >> 16);
  }
  return true;
}

static bool parse_slideshow_duration(struct imv *imv, const char *duration)
{
  char *decimal;
  imv->slideshow.duration = strtod(duration, &decimal);
  return true;
}

static bool parse_scaling_mode(struct imv *imv, const char *mode)
{
  if (!strcmp(mode, "shrink")) {
    imv->scaling_mode = SCALING_DOWN;
    return true;
  }

  if (!strcmp(mode, "full")) {
    imv->scaling_mode = SCALING_FULL;
    return true;
  }

  if (!strcmp(mode, "none")) {
    imv->scaling_mode = SCALING_NONE;
    return true;
  }

  return false;
}

static bool parse_upscaling_method(struct imv *imv, const char *method)
{
  if (!strcmp(method, "linear")) {
    imv->upscaling_method = UPSCALING_LINEAR;
    return true;
  }

  if (!strcmp(method, "nearest_neighbour")) {
    imv->upscaling_method = UPSCALING_NEAREST_NEIGHBOUR;
    return true;
  }

  return false;
}

static void *load_paths_from_stdin(void *data)
{
  struct imv *imv = data;

  imv_log(IMV_INFO, "Reading paths from stdin...");

  char buf[PATH_MAX];
  while (fgets(buf, sizeof(buf), stdin) != NULL) {
    size_t len = strlen(buf);
    if (buf[len-1] == '\n') {
      buf[--len] = 0;
    }
    if (len > 0) {
      struct internal_event *event = calloc(1, sizeof *event);
      event->type = NEW_PATH;
      event->data.new_path.path = strdup(buf);

      struct imv_event e = {
        .type = IMV_EVENT_CUSTOM,
        .data = {
          .custom = event
        }
      };
      imv_window_push_event(imv->window, &e);
    }
  }
  return NULL;
}

static void print_help(struct imv *imv)
{
  printf("imv %s\nSee manual for usage information.\n", IMV_VERSION);
  puts("This version of imv has been compiled with the following backends:\n");

  for (struct backend_chain *chain = imv->backends;
       chain;
       chain = chain->next) {
    printf("Name: %s\n"
           "Description: %s\n"
           "Website: %s\n"
           "License: %s\n\n",
           chain->backend->name,
           chain->backend->description,
           chain->backend->website,
           chain->backend->license);
  }

  puts("Legal:\n"
       "imv's full source code is published under the terms of the MIT\n"
       "license, and can be found at https://github.com/eXeC64/imv\n"
       "\n"
       "imv uses the inih library to parse ini files.\n"
       "See https://github.com/benhoyt/inih for details.\n"
       "inih is used under the New (3-clause) BSD license.");
}

bool imv_parse_args(struct imv *imv, int argc, char **argv)
{
  /* Do not print getopt errors */
  opterr = 0;

  int o;

 /* TODO getopt_long */
  while ((o = getopt(argc, argv, "frdxhvlu:s:n:b:t:")) != -1) {
    switch(o) {
      case 'f': imv->start_fullscreen = true;                    break;
      case 'r': imv->recursive_load = true;                      break;
      case 'd': imv->overlay_enabled = true;                     break;
      case 'x': imv->loop_input = false;                         break;
      case 'l': imv->list_files_at_exit = true;                  break;
      case 'n': imv->starting_path = optarg;                     break;
      case 'h':
        print_help(imv);
        imv->quit = true;
        return true;
      case 'v':
        printf("Version: %s\n", IMV_VERSION);
          imv->quit = true;
          return false;
      case 's':
        if (!parse_scaling_mode(imv, optarg)) {
          imv_log(IMV_ERROR, "Invalid scaling mode. Aborting.\n");
          return false;
        }
        break;
      case 'u':
        if (!parse_upscaling_method(imv, optarg)) {
          imv_log(IMV_ERROR, "Invalid upscaling method. Aborting.\n");
          return false;
        }
        break;
      case 'b':
        if (!parse_bg(imv, optarg)) {
          imv_log(IMV_ERROR, "Invalid background. Aborting.\n");
          return false;
        }
        break;
      case 't':
        if (!parse_slideshow_duration(imv, optarg)) {
          imv_log(IMV_ERROR, "Invalid slideshow duration. Aborting.\n");
          return false;
        }
        break;
      case '?':
        imv_log(IMV_ERROR, "Unknown argument '%c'. Aborting.\n", optopt);
        return false;
    }
  }

  argc -= optind;
  argv += optind;

  /* if no paths are given as args, expect them from stdin */
  if (argc == 0) {
    imv->paths_from_stdin = true;
  } else {
    /* otherwise, add the paths */
    bool data_from_stdin = false;
    for (int i = 0; i < argc; ++i) {

      /* Special case: '-' denotes reading image data from stdin */
      if (!strcmp("-", argv[i])) {
        if (imv->paths_from_stdin) {
          imv_log(IMV_ERROR, "Can't read paths AND image data from stdin. Aborting.\n");
          return false;
        } else if (data_from_stdin) {
          imv_log(IMV_ERROR, "Can't read image data from stdin twice. Aborting.\n");
          return false;
        }
        data_from_stdin = true;

        imv->stdin_image_data_len = read_from_stdin(&imv->stdin_image_data);
      }

      imv_add_path(imv, argv[i]);
    }
  }

  return true;
}

void imv_add_path(struct imv *imv, const char *path)
{
  imv_navigator_add(imv->navigator, path, imv->recursive_load);
}

int imv_run(struct imv *imv)
{
  if (imv->quit)
    return 0;

  if (!setup_window(imv))
    return 1;

  /* if loading paths from stdin, kick off a thread to do that - we'll receive
   * events back via internal events */
  if (imv->paths_from_stdin) {
    pthread_t thread;
    pthread_create(&thread, NULL, load_paths_from_stdin, imv);
    pthread_detach(thread);
  }

  if (imv->starting_path) {
    ssize_t index = imv_navigator_find_path(imv->navigator, imv->starting_path);
    if (index == -1) {
      index = (int) strtol(imv->starting_path, NULL, 10);
      index -= 1; /* input is 1-indexed, internally we're 0 indexed */
      if (errno == EINVAL) {
        index = -1;
      }
    }

    if (index >= 0) {
      imv_navigator_select_abs(imv->navigator, index);
    } else {
      imv_log(IMV_ERROR, "Invalid starting image: %s\n", imv->starting_path);
    }
  }

  /* time keeping */
  double last_time = cur_time();
  double current_time;


  while (!imv->quit) {

    /* Check if navigator wrapped around paths lists */
    if (!imv->loop_input && imv_navigator_wrapped(imv->navigator)) {
      break;
    }

    /* If the user has changed image, start loading the new one. It's possible
     * that there are lots of unsupported files listed back to back, so we
     * may immediate close one and navigate onto the next. So we attempt to
     * load in a while loop until the navigation stops.
     */
    while (imv_navigator_poll_changed(imv->navigator)) {
      const char *current_path = imv_navigator_selection(imv->navigator);
      /* check we got a path back */
      if (strcmp("", current_path)) {

        const bool path_is_stdin = !strcmp("-", current_path);
        struct imv_source *new_source;

        enum backend_result result = BACKEND_UNSUPPORTED;

        if (!imv->backends) {
          imv_log(IMV_ERROR, "No backends installed. Unable to load image.\n");
        }
        for (struct backend_chain *chain = imv->backends; chain; chain = chain->next) {
          const struct imv_backend *backend = chain->backend;
          if (path_is_stdin) {

            if (!backend->open_memory) {
              /* memory loading unsupported by backend */
              continue;
            }

            result = backend->open_memory(imv->stdin_image_data,
                imv->stdin_image_data_len, &new_source);
          } else {

            if (!backend->open_path) {
              /* path loading unsupported by backend */
              continue;
            }

            result = backend->open_path(current_path, &new_source);
          }
          if (result == BACKEND_UNSUPPORTED) {
            /* Try the next backend */
            continue;
          } else {
            break;
          }
        }

        if (result == BACKEND_SUCCESS) {
          if (imv->current_source) {
            async_free_source(imv->current_source);
          }
          imv->current_source = new_source;
          imv->current_source->callback = &source_callback;
          imv->current_source->user_data = imv;
          async_load_first_frame(imv->current_source);

          imv->loading = true;
          imv_viewport_set_playing(imv->view, true);

          char title[1024];
          generate_env_text(imv, title, sizeof title, imv->title_text);
          imv_window_set_title(imv->window, title);
        } else {
          /* Error loading path so remove it from the navigator */
          imv_navigator_remove(imv->navigator, current_path);
        }
      } else {
        /* No image currently selected */
        if (imv->current_image) {
          imv_image_free(imv->current_image);
          imv->current_image = NULL;
        }
      }
    }

    if (imv->need_rescale) {
      int ww, wh;
      imv_window_get_size(imv->window, &ww, &wh);

      imv->need_rescale = false;
      if (imv->scaling_mode == SCALING_NONE ||
          (imv->scaling_mode == SCALING_DOWN
           && ww > imv_image_width(imv->current_image)
           && wh > imv_image_height(imv->current_image))) {
        imv_viewport_scale_to_actual(imv->view, imv->current_image);
      } else {
        imv_viewport_scale_to_window(imv->view, imv->current_image);
      }
    }

    current_time = cur_time();

    /* Check if a new frame is due */
    bool should_change_frame = false;
    if (imv->next_frame.force_next_frame && imv->next_frame.image) {
      should_change_frame = true;
    }
    if (imv_viewport_is_playing(imv->view) && imv->next_frame.image
        && imv->next_frame.due && imv->next_frame.due <= current_time) {
      should_change_frame = true;
    }

    if (should_change_frame) {
      if (imv->current_image) {
        imv_image_free(imv->current_image);
      }
      imv->current_image = imv->next_frame.image;
      imv->next_frame.image = NULL;
      imv->next_frame.due = current_time + imv->next_frame.duration;
      imv->next_frame.duration = 0;
      imv->next_frame.force_next_frame = false;

      imv->need_redraw = true;

      /* Trigger loading of a new frame, now this one's being displayed */
      if (imv->current_source && imv->current_source->load_next_frame) {
        async_load_next_frame(imv->current_source);
      }
    }

    /* handle slideshow */
    if (imv->slideshow.duration != 0.0) {
      double dt = current_time - last_time;

      imv->slideshow.elapsed += dt;
      if (imv->slideshow.elapsed >= imv->slideshow.duration) {
        imv_navigator_select_rel(imv->navigator, 1);
        imv->slideshow.elapsed = 0;
        imv->need_redraw = true;
      }
    }

    last_time = current_time;

    /* check if the viewport needs a redraw */
    if (imv_viewport_needs_redraw(imv->view)) {
      imv->need_redraw = true;
    }

    if (imv->need_redraw) {
      imv_window_clear(imv->window, 0, 0, 0);
      render_window(imv);
      imv_window_present(imv->window);
    }

    /* sleep until we have something to do */
    double timeout = 1.0; /* seconds */

    /* If we need to display the next frame of an animation soon we should
     * limit our sleep until the next frame is due.
     */
    if (imv_viewport_is_playing(imv->view) && imv->next_frame.due != 0.0) {
      timeout = imv->next_frame.due - current_time;
      if (timeout < 0.001) {
        timeout = 0.001;
      }
    }

    if (imv->slideshow.duration > 0) {
      double timeleft = imv->slideshow.duration - imv->slideshow.elapsed;
      if (timeleft > 0.0 && timeleft < timeout) {
        timeout = timeleft + 0.001;
      }
    }

    /* Go to sleep until an input/internal event or the timeout expires */
    imv_window_wait_for_event(imv->window, timeout);

    /* Handle the new events that have arrived */
    imv_window_pump_events(imv->window, event_handler, imv);
  }

  if (imv->list_files_at_exit) {
    for (size_t i = 0; i < imv_navigator_length(imv->navigator); ++i)
      puts(imv_navigator_at(imv->navigator, i));
  }

  return 0;
}

static bool setup_window(struct imv *imv)
{
  imv->window = imv_window_create(imv->initial_width, imv->initial_height, "imv");

  if (!imv->window) {
    imv_log(IMV_ERROR, "Failed to create window\n");
    return false;
  }

  {
    int ww, wh, bw, bh;
    imv_window_get_size(imv->window, &ww, &wh);
    imv_window_get_framebuffer_size(imv->window, &bw, &bh);
    imv->view = imv_viewport_create(ww, wh, bw, bh);
  }

  /* put us in fullscren mode to begin with if requested */
  imv_window_set_fullscreen(imv->window, imv->start_fullscreen);

  {
    imv->keyboard = imv_keyboard_create();
    assert(imv->keyboard);

    const char *keymap = imv_window_keymap(imv->window);
    if (keymap) {
      imv_keyboard_set_keymap(imv->keyboard, keymap);
    }
  }

  {
    int ww, wh;
    imv_window_get_size(imv->window, &ww, &wh);
    imv->canvas = imv_canvas_create(ww, wh);
    imv_canvas_font(imv->canvas, imv->font.name, imv->font.size);
  }

  return true;
}


static void handle_new_image(struct imv *imv, struct imv_image *image, int frametime)
{
  if (imv->current_image) {
    imv_image_free(imv->current_image);
  }
  imv->current_image = image;
  imv->need_redraw = true;
  imv->need_rescale = true;
  imv->loading = false;
  imv->next_frame.due = frametime ? cur_time() + frametime * 0.001 : 0.0;
  imv->next_frame.duration = 0.0;

  /* If this is an animated image, we should kick off loading the next frame */
  if (imv->current_source && imv->current_source->load_next_frame && frametime) {
    async_load_next_frame(imv->current_source);
  }
}

static void handle_new_frame(struct imv *imv, struct imv_image *image, int frametime)
{
  if (imv->next_frame.image) {
    imv_image_free(imv->next_frame.image);
  }
  imv->next_frame.image = image;

  imv->next_frame.duration = frametime * 0.001;
}

static void consume_internal_event(struct imv *imv, struct internal_event *event)
{
  if (event->type == NEW_IMAGE) {
    /* New image vs just a new frame of the same image */
    if (event->data.new_image.is_new_image) {
      handle_new_image(imv, event->data.new_image.image, event->data.new_image.frametime);
    } else {
      handle_new_frame(imv, event->data.new_image.image, event->data.new_image.frametime);
    }

  } else if (event->type == BAD_IMAGE) {
    /* An image failed to load, remove it from our image list */
    const char *err_path = imv_navigator_selection(imv->navigator);

    /* Special case: the image came from stdin */
    if (strcmp(err_path, "-") == 0) {
      if (imv->stdin_image_data) {
        free(imv->stdin_image_data);
        imv->stdin_image_data = NULL;
        imv->stdin_image_data_len = 0;
      }
      imv_log(IMV_ERROR, "Failed to load image from stdin.\n");
    }

    imv_navigator_remove(imv->navigator, err_path);

  } else if (event->type == NEW_PATH) {
    /* Received a new path from the stdin reading thread */
    imv_add_path(imv, event->data.new_path.path);
    free(event->data.new_path.path);
    /* Need to update image count in title */
    imv->need_redraw = true;

  } else if (event->type == COMMAND) {
    struct list *commands = list_create();
    list_append(commands, event->data.command.text);
    imv_command_exec_list(imv->commands, commands, imv);
    list_deep_free(commands);
    imv->need_redraw = true;
  }

  free(event);
  return;
}

static void render_window(struct imv *imv)
{
  int ww, wh;
  imv_window_get_size(imv->window, &ww, &wh);

  /* update window title */
  char title_text[1024];
  generate_env_text(imv, title_text, sizeof title_text, imv->title_text);
  imv_window_set_title(imv->window, title_text);

  /* first we draw the background */
  if (imv->background.type == BACKGROUND_SOLID) {
    imv_canvas_clear(imv->canvas);
    imv_canvas_color(imv->canvas,
        imv->background.color.r / 255.f,
        imv->background.color.g / 255.f,
        imv->background.color.b / 255.f,
        1.0);
    imv_canvas_fill(imv->canvas);
    imv_canvas_draw(imv->canvas);
  } else {
    /* chequered background */
    imv_canvas_fill_checkers(imv->canvas, 16);
    imv_canvas_draw(imv->canvas);
  }

  /* draw our actual image */
  if (imv->current_image) {
    int x, y;
    double scale;
    imv_viewport_get_offset(imv->view, &x, &y);
    imv_viewport_get_scale(imv->view, &scale);
    imv_canvas_draw_image(imv->canvas, imv->current_image, x, y, scale, imv->upscaling_method);
  }

  imv_canvas_clear(imv->canvas);

  /* if the overlay needs to be drawn, draw that too */
  if (imv->overlay_enabled) {
    const int height = imv->font.size * 1.2;
    imv_canvas_color(imv->canvas, 0, 0, 0, 0.75);
    imv_canvas_fill_rectangle(imv->canvas, 0, 0, ww, height);
    imv_canvas_color(imv->canvas, 1, 1, 1, 1);
    char overlay_text[1024];
    generate_env_text(imv, overlay_text, sizeof overlay_text, imv->overlay_text);
    imv_canvas_printf(imv->canvas, 0, 0, "%s", overlay_text);
  }

  /* draw command entry bar if needed */
  if (imv_console_prompt(imv->console)) {
    const int bottom_offset = 5;
    const int height = imv->font.size * 1.2;
    imv_canvas_color(imv->canvas, 0, 0, 0, 0.75);
    imv_canvas_fill_rectangle(imv->canvas, 0, wh - height - bottom_offset,
        ww, height + bottom_offset);
    imv_canvas_color(imv->canvas, 1, 1, 1, 1);
    imv_canvas_printf(imv->canvas, 0, wh - height - bottom_offset,
        ":%s\u2588", imv_console_prompt(imv->console));
  }

  imv_canvas_draw(imv->canvas);

  /* redraw complete, unset the flag */
  imv->need_redraw = false;
}

static char *get_config_path(void)
{
  const char *config_paths[] = {
    "$imv_config",
    "$XDG_CONFIG_HOME/imv/config",
    "$HOME/.config/imv/config",
    "$HOME/.imv_config",
    "$HOME/.imv/config",
    "/usr/local/etc/imv_config",
    "/etc/imv_config",
  };

  for (size_t i = 0; i < sizeof(config_paths) / sizeof(char*); ++i) {
    wordexp_t word;
    if (wordexp(config_paths[i], &word, 0) == 0) {
      if (!word.we_wordv[0]) {
        wordfree(&word);
        continue;
      }

      char *path = strdup(word.we_wordv[0]);
      wordfree(&word);

      if (!path || access(path, R_OK) == -1) {
        free(path);
        continue;
      }

      return path;
    }
  }
  return NULL;
}

static bool parse_bool(const char *str)
{
  return (
    !strcmp(str, "1") ||
    !strcmp(str, "yes") ||
    !strcmp(str, "true") ||
    !strcmp(str, "on")
  );
}

static int handle_ini_value(void *user, const char *section, const char *name,
                            const char *value)
{
  struct imv *imv = user;

  if (!strcmp(section, "binds")) {
    return add_bind(imv, name, value);
  }

  if (!strcmp(section, "aliases")) {
    imv_command_alias(imv->commands, name, value);
    return 1;
  }

  if (!strcmp(section, "options")) {

    if (!strcmp(name, "fullscreen")) {
      imv->start_fullscreen = parse_bool(value);
      return 1;
    }

    if (!strcmp(name, "width")) {
      imv->initial_width = strtol(value, NULL, 10);
      return 1;
    }
    if (!strcmp(name, "height")) {
      imv->initial_height = strtol(value, NULL, 10);
      return 1;
    }

    if (!strcmp(name, "overlay")) {
      imv->overlay_enabled = parse_bool(value);
      return 1;
    }

    if (!strcmp(name, "upscaling_method")) {
      return parse_upscaling_method(imv, value);
    }

    if (!strcmp(name, "recursive")) {
      imv->recursive_load = parse_bool(value);
      return 1;
    }

    if (!strcmp(name, "loop_input")) {
      imv->loop_input = parse_bool(value);
      return 1;
    }

    if (!strcmp(name, "list_files_at_exit")) {
      imv->list_files_at_exit = parse_bool(value);
      return 1;
    }

    if (!strcmp(name, "scaling_mode")) {
      return parse_scaling_mode(imv, value);
    }

    if (!strcmp(name, "background")) {
      if (!parse_bg(imv, value)) {
        return false;
      }
      return 1;
    }

    if (!strcmp(name, "slideshow_duration")) {
      if (!parse_slideshow_duration(imv, value)) {
        return false;
      }
      return 1;
    }

    if (!strcmp(name, "overlay_font")) {
      free(imv->font.name);
      imv->font.name = strdup(value);
      char *sep = strchr(imv->font.name, ':');
      if (sep) {
        *sep = 0;
        imv->font.size = atoi(sep + 1);
      } else {
        imv->font.size = 24;
      }
      return 1;
    }

    if (!strcmp(name, "overlay_text")) {
      free(imv->overlay_text);
      imv->overlay_text = strdup(value);
      return 1;
    }

    if (!strcmp(name, "title_text")) {
      free(imv->title_text);
      imv->title_text = strdup(value);
      return 1;
    }

    if (!strcmp(name, "suppress_default_binds")) {
      const bool suppress_default_binds = parse_bool(value);
      if (suppress_default_binds) {
        /* clear out any default binds if requested */
        imv_binds_clear(imv->binds);
      }
      return 1;
    }

    /* No matches so far */
    imv_log(IMV_WARNING, "Ignoring unknown option: %s\n", name);
    return 1;
  }
  return 0;
}

bool imv_load_config(struct imv *imv)
{
  char *path = get_config_path();
  if (!path) {
    /* no config, no problem - we have defaults */
    return true;
  }

  bool result = true;

  const int err = ini_parse(path, handle_ini_value, imv);
  if (err == -1) {
    imv_log(IMV_ERROR, "Unable to open config file: %s\n", path);
    result = false;
  } else if (err > 0) {
    imv_log(IMV_ERROR, "Error in config file: %s:%d\n", path, err);
    result = false;
  }
  free(path);
  return result;
}

static void command_quit(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  imv->quit = true;
}

static void command_pan(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  if (args->len != 3) {
    return;
  }

  long int x = strtol(args->items[1], NULL, 10);
  long int y = strtol(args->items[2], NULL, 10);

  imv_viewport_move(imv->view, x, y, imv->current_image);
}

static void command_next(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  long int index = 1;

  if (args->len >= 2) {
    index = strtol(args->items[1], NULL, 10);
  }

  imv_navigator_select_rel(imv->navigator, index);

  imv->slideshow.elapsed = 0;
}

static void command_prev(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  long int index = 1;

  if (args->len >= 2) {
    index = strtol(args->items[1], NULL, 10);
  }

  imv_navigator_select_rel(imv->navigator, -index);

  imv->slideshow.elapsed = 0;
}

static void command_goto(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  if (args->len != 2) {
    return;
  }

  long int index = strtol(args->items[1], NULL, 10);
  imv_navigator_select_abs(imv->navigator, index - 1);

  imv->slideshow.elapsed = 0;
}

static void command_zoom(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  if (args->len == 2) {
    const char *str = args->items[1];
    if (!strcmp(str, "actual")) {
      imv_viewport_scale_to_actual(imv->view, imv->current_image);
    } else {
      long int amount = strtol(args->items[1], NULL, 10);
      imv_viewport_zoom(imv->view, imv->current_image, IMV_ZOOM_KEYBOARD, 0, 0, amount);
    }
  }
}

static void command_open(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  bool recursive = imv->recursive_load;

  update_env_vars(imv);
  for (size_t i = 1; i < args->len; ++i) {

    /* allow -r arg to specify recursive */
    if (i == 1 && !strcmp(args->items[i], "-r")) {
      recursive = true;
      continue;
    }

    wordexp_t word;
    if (wordexp(args->items[i], &word, 0) == 0) {
      for (size_t j = 0; j < word.we_wordc; ++j) {
        imv_navigator_add(imv->navigator, word.we_wordv[j], recursive);
      }
      wordfree(&word);
    }
  }
}

static void command_close(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  char* path = strdup(imv_navigator_selection(imv->navigator));
  imv_navigator_remove(imv->navigator, path);
  free(path);

  imv->slideshow.elapsed = 0;
}

static void command_fullscreen(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;

  imv_window_set_fullscreen(imv->window, !imv_window_is_fullscreen(imv->window));
}

static void command_overlay(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  imv->overlay_enabled = !imv->overlay_enabled;
  imv->need_redraw = true;
}

static void command_exec(struct list *args, const char *argstr, void *data)
{
  (void)args;
  struct imv *imv = data;
  update_env_vars(imv);
  system(argstr);
}

static void command_center(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  imv_viewport_center(imv->view, imv->current_image);
}

static void command_reset(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  imv->need_rescale = true;
  imv->need_redraw = true;
}

static void command_next_frame(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  if (imv->current_source && imv->current_source->load_next_frame) {
    async_load_next_frame(imv->current_source);
    imv->next_frame.force_next_frame = true;
  }
}

static void command_toggle_playing(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  imv_viewport_toggle_playing(imv->view);
}

static void command_set_scaling_mode(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;

  if (args->len != 2) {
    return;
  }

  const char *mode = args->items[1];

  if (!strcmp(mode, "next")) {
    imv->scaling_mode++;
    imv->scaling_mode %= SCALING_MODE_COUNT;
  } else if (!strcmp(mode, "none")) {
    imv->scaling_mode = SCALING_NONE;
  } else if (!strcmp(mode, "shrink")) {
    imv->scaling_mode = SCALING_DOWN;
  } else if (!strcmp(mode, "full")) {
    imv->scaling_mode = SCALING_FULL;
  } else {
    /* no changes, don't bother to redraw */
    return;
  }

  imv->need_rescale = true;
  imv->need_redraw = true;
}

static void command_set_slideshow_duration(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  if (args->len == 2) {
    const char *arg = args->items[1];
    const char prefix = *arg;

    int new_duration = imv->slideshow.duration;

    long int arg_num = strtol(arg, NULL, 10);

    if (prefix == '+' || prefix == '-') {
      new_duration += arg_num;
    } else {
      new_duration = arg_num;
    }

    if (new_duration < 0) {
      new_duration = 0;
    }

    imv->slideshow.duration = new_duration;
    imv->need_redraw = true;
  }
}

static void command_set_background(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  if (args->len == 2) {
    parse_bg(imv, args->items[1]);
  }
}

static void update_env_vars(struct imv *imv)
{
  char str[64];

  snprintf(str, sizeof str, "%d", getpid());
  setenv("imv_pid", str, 1);

  setenv("imv_current_file", imv_navigator_selection(imv->navigator), 1);
  setenv("imv_scaling_mode", scaling_label[imv->scaling_mode], 1);
  setenv("imv_loading", imv->loading ? "1" : "0", 1);

  if (imv_navigator_length(imv->navigator)) {
    snprintf(str, sizeof str, "%zu", imv_navigator_index(imv->navigator) + 1);
    setenv("imv_current_index", str, 1);
  } else {
    setenv("imv_current_index", "0", 1);
  }

  snprintf(str, sizeof str, "%zu", imv_navigator_length(imv->navigator));
  setenv("imv_file_count", str, 1);

  snprintf(str, sizeof str, "%d", imv_image_width(imv->current_image));
  setenv("imv_width", str, 1);

  snprintf(str, sizeof str, "%d", imv_image_height(imv->current_image));
  setenv("imv_height", str, 1);

  {
    double scale;
    imv_viewport_get_scale(imv->view, &scale);
    snprintf(str, sizeof str, "%d", (int)(scale * 100.0));
    setenv("imv_scale", str, 1);
  }

  snprintf(str, sizeof str, "%f", imv->slideshow.duration);
  setenv("imv_slideshow_duration", str, 1);

  snprintf(str, sizeof str, "%f", imv->slideshow.elapsed);
  setenv("imv_slideshow_elapsed", str, 1);
}

static size_t generate_env_text(struct imv *imv, char *buf, size_t buf_len, const char *format)
{
  update_env_vars(imv);

  size_t len = 0;
  wordexp_t word;
  if (wordexp(format, &word, 0) == 0) {
    for (size_t i = 0; i < word.we_wordc; ++i) {
      len += snprintf(buf + len, buf_len - len, "%s ", word.we_wordv[i]);
    }
    wordfree(&word);
  } else {
    len += snprintf(buf, buf_len, "error expanding text");
  }

  return len;
}

static size_t read_from_stdin(void **buffer)
{
  size_t len = 0;
  ssize_t r;
  size_t step = 4096; /* Arbitrary value of 4 KiB */
  void *p;

  errno = 0; /* clear errno */

  for (*buffer = NULL; (*buffer = realloc((p = *buffer), len + step));
      len += (size_t)r) {
    if ((r = read(STDIN_FILENO, (uint8_t *)*buffer + len, step)) == -1) {
      perror(NULL);
      break;
    } else if (r == 0) {
      break;
    }
  }

  /* realloc(3) leaves old buffer allocated in case of error */
  if (*buffer == NULL && p != NULL) {
    int save = errno;
    free(p);
    errno = save;
    len = 0;
  }
  return len;
}


/* vim:set ts=2 sts=2 sw=2 et: */
