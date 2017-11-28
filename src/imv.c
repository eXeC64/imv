#include "imv.h"

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <wordexp.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "binds.h"
#include "commands.h"
#include "ini.h"
#include "list.h"
#include "loader.h"
#include "image.h"
#include "navigator.h"
#include "viewport.h"
#include "util.h"

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

struct imv {
  bool quit;
  bool fullscreen;
  bool overlay_enabled;
  bool nearest_neighbour;
  bool need_redraw;
  bool need_rescale;
  bool recursive_load;
  bool loop_input;
  bool list_files_at_exit;
  bool paths_from_stdin;
  enum scaling_mode scaling_mode;
  enum background_type background_type;
  struct { unsigned char r, g, b; } background_color;
  unsigned long slideshow_image_duration;
  unsigned long slideshow_time_elapsed;
  char *font_name;
  struct imv_binds *binds;
  struct imv_navigator *navigator;
  struct imv_loader *loader;
  struct imv_commands *commands;
  struct imv_image *image;
  struct imv_viewport *view;
  void *stdin_image_data;
  size_t stdin_image_data_len;
  char *input_buffer;
  char *starting_path;

  SDL_Window *window;
  SDL_Renderer *renderer;
  TTF_Font *font;
  SDL_Texture *background_image;
  bool sdl_init;
  bool ttf_init;
  struct {
    unsigned int NEW_IMAGE;
    unsigned int BAD_IMAGE;
    unsigned int NEW_PATH;
  } events;
  struct {
    int width;
    int height;
  } current_image;
};

void command_quit(struct list *args, const char *argstr, void *data);
void command_pan(struct list *args, const char *argstr, void *data);
void command_select_rel(struct list *args, const char *argstr, void *data);
void command_select_abs(struct list *args, const char *argstr, void *data);
void command_zoom(struct list *args, const char *argstr, void *data);
void command_remove(struct list *args, const char *argstr, void *data);
void command_fullscreen(struct list *args, const char *argstr, void *data);
void command_overlay(struct list *args, const char *argstr, void *data);
void command_exec(struct list *args, const char *argstr, void *data);
void command_center(struct list *args, const char *argstr, void *data);
void command_reset(struct list *args, const char *argstr, void *data);
void command_next_frame(struct list *args, const char *argstr, void *data);
void command_toggle_playing(struct list *args, const char *argstr, void *data);
void command_set_scaling_mode(struct list *args, const char *argstr, void *data);
void command_set_slideshow_duration(struct list *args, const char *argstr, void *data);

static bool setup_window(struct imv *imv);
static void handle_event(struct imv *imv, SDL_Event *event);
static void render_window(struct imv *imv);

static const char *add_bind(struct imv *imv, const char *keys, const char *command)
{
  struct list *list = imv_bind_parse_keys(keys);
  if(!list) {
    return "Invalid key combination";
  }

  enum bind_result result = imv_binds_add(imv->binds, list, command);

  if (result == BIND_SUCCESS) {
    return NULL;
  } else if (result == BIND_INVALID_KEYS) {
    return "Invalid keys to bind to";
  } else if (result == BIND_INVALID_COMMAND) {
    return "No command given to bind to";
  } else if (result == BIND_CONFLICTS) {
    return "Key combination conflicts with existing bind";
  }

  return NULL;
}

struct imv *imv_create(void)
{
  struct imv *imv = malloc(sizeof(struct imv));
  imv->quit = false;
  imv->fullscreen = false;
  imv->overlay_enabled = false;
  imv->nearest_neighbour = false;
  imv->need_redraw = true;
  imv->need_rescale = true;
  imv->recursive_load = false;
  imv->scaling_mode = SCALING_FULL;
  imv->loop_input = true;
  imv->list_files_at_exit = false;
  imv->paths_from_stdin = false;
  imv->background_color.r = imv->background_color.g = imv->background_color.b = 0;
  imv->slideshow_image_duration = 0;
  imv->slideshow_time_elapsed = 0;
  imv->font_name = strdup("Monospace:24");
  imv->binds = imv_binds_create();
  imv->navigator = imv_navigator_create();
  imv->loader = imv_loader_create();
  imv->commands = imv_commands_create();
  imv->stdin_image_data = NULL;
  imv->stdin_image_data_len = 0;
  imv->input_buffer = NULL;
  imv->starting_path = NULL;
  imv->window = NULL;
  imv->renderer = NULL;
  imv->font = NULL;
  imv->background_image = NULL;
  imv->sdl_init = false;
  imv->ttf_init = false;

  imv_command_register(imv->commands, "quit", &command_quit);
  imv_command_register(imv->commands, "pan", &command_pan);
  imv_command_register(imv->commands, "select_rel", &command_select_rel);
  imv_command_register(imv->commands, "select_abs", &command_select_abs);
  imv_command_register(imv->commands, "zoom", &command_zoom);
  imv_command_register(imv->commands, "remove", &command_remove);
  imv_command_register(imv->commands, "fullscreen", &command_fullscreen);
  imv_command_register(imv->commands, "overlay", &command_overlay);
  imv_command_register(imv->commands, "exec", &command_exec);
  imv_command_register(imv->commands, "center", &command_center);
  imv_command_register(imv->commands, "reset", &command_reset);
  imv_command_register(imv->commands, "next_frame", &command_next_frame);
  imv_command_register(imv->commands, "toggle_playing", &command_toggle_playing);
  imv_command_register(imv->commands, "scaling_mode", &command_set_scaling_mode);
  imv_command_register(imv->commands, "slideshow_duration", &command_set_slideshow_duration);

  add_bind(imv, "<q>", "quit");
  add_bind(imv, "<Left>", "select_rel -1");
  add_bind(imv, "<LeftSquareBracket>", "select_rel -1");
  add_bind(imv, "<Right>", "select_rel 1");
  add_bind(imv, "<RightSquareBracket>", "select_rel 1");
  add_bind(imv, "<g><g>", "select_abs 0");
  add_bind(imv, "<Shift+g>", "select_abs -1");
  add_bind(imv, "<j>", "pan 0 -50");
  add_bind(imv, "<k>", "pan 0 50");
  add_bind(imv, "<h>", "pan 50 0");
  add_bind(imv, "<l>", "pan -50 0");
  add_bind(imv, "<x>", "remove");
  add_bind(imv, "<f>", "fullscreen");
  add_bind(imv, "<d>", "overlay");
  add_bind(imv, "<p>", "exec echo $imv_current_file");
  add_bind(imv, "<Equals>", "zoom 1");
  add_bind(imv, "<Up>", "zoom 1");
  add_bind(imv, "<+>", "zoom 1");
  add_bind(imv, "<i>", "zoom 1");
  add_bind(imv, "<Down>", "zoom -1");
  add_bind(imv, "<->", "zoom -1");
  add_bind(imv, "<o>", "zoom -1");
  add_bind(imv, "<c>", "center");
  add_bind(imv, "<s>", "scaling_mode next");
  add_bind(imv, "<a>", "zoom actual");
  add_bind(imv, "<r>", "reset");
  add_bind(imv, "<.>", "next_frame");
  add_bind(imv, "<Space>", "toggle_playing");
  add_bind(imv, "<t>", "slideshow_duration +1");
  add_bind(imv, "<Shift+t>", "slideshow_duration -1");

  return imv;
}

void imv_free(struct imv *imv)
{
  free(imv->font_name);
  imv_binds_free(imv->binds);
  imv_navigator_free(imv->navigator);
  imv_loader_free(imv->loader);
  imv_commands_free(imv->commands);
  if(imv->stdin_image_data) {
    free(imv->stdin_image_data);
  }
  if(imv->input_buffer) {
    free(imv->input_buffer);
  }
  if(imv->renderer) {
    SDL_DestroyRenderer(imv->renderer);
  }
  if(imv->window) {
    SDL_DestroyWindow(imv->window);
  }
  if(imv->background_image) {
    SDL_DestroyTexture(imv->background_image);
  }
  if(imv->font) {
    TTF_CloseFont(imv->font);
  }
  if(imv->ttf_init) {
    TTF_Quit();
  }
  if(imv->sdl_init) {
    SDL_Quit();
  }
  free(imv);
}

static bool parse_bg(struct imv *imv, const char *bg)
{
  if(strcmp("checks", bg) == 0) {
    imv->background_type = BACKGROUND_CHEQUERED;
  } else {
    imv->background_type = BACKGROUND_SOLID;
    if(*bg == '#')
      ++bg;
    char *ep;
    uint32_t n = strtoul(bg, &ep, 16);
    if(*ep != '\0' || ep - bg != 6 || n > 0xFFFFFF) {
      fprintf(stderr, "Invalid hex color: '%s'\n", bg);
      return false;
    }
    imv->background_color.b = n & 0xFF;
    imv->background_color.g = (n >> 8) & 0xFF;
    imv->background_color.r = (n >> 16);
  }
  return true;
}

static bool parse_slideshow_duration(struct imv *imv, const char *duration)
{
  char *decimal;
  imv->slideshow_image_duration = strtoul(duration, &decimal, 10);
  imv->slideshow_image_duration *= 1000;
  if (*decimal == '.') {
    char *ep;
    long delay = strtoul(++decimal, &ep, 10);
    for (int i = 3 - (ep - decimal); i; i--) {
      delay *= 10;
    }
    if (delay < 1000) {
      imv->slideshow_image_duration += delay;
    } else {
      imv->slideshow_image_duration = ULONG_MAX;
    }
  }
  if (imv->slideshow_image_duration == ULONG_MAX) {
    fprintf(stderr, "Wrong slideshow duration '%s'. Aborting.\n", optarg);
    return false;
  }
  return true;
}

static int load_paths_from_stdin(void *data)
{
  struct imv *imv = data;

  fprintf(stderr, "Reading paths from stdin...");

  char buf[PATH_MAX];
  while(fgets(buf, sizeof(buf), stdin) != NULL) {
    size_t len = strlen(buf);
    if(buf[len-1] == '\n') {
      buf[--len] = 0;
    }
    if(len > 0) {
      /* return the path via SDL event queue */
      SDL_Event event;
      SDL_zero(event);
      event.type = imv->events.NEW_PATH;
      event.user.data1 = strdup(buf);
      SDL_PushEvent(&event);
    }
  }
  return 0;
}

bool imv_parse_args(struct imv *imv, int argc, char **argv)
{
  /* Do not print getopt errors */
  opterr = 0;

  int o;

  while((o = getopt(argc, argv, "frasSudxhln:b:e:t:")) != -1) {
    switch(o) {
      case 'f': imv->fullscreen = true;            break;
      case 'r': imv->recursive_load = true;        break;
      case 'a': imv->scaling_mode = SCALING_NONE;  break;
      case 's': imv->scaling_mode = SCALING_DOWN;  break;
      case 'S': imv->scaling_mode = SCALING_FULL;  break;
      case 'u': imv->nearest_neighbour = true;     break;
      case 'd': imv->overlay_enabled = true;       break;
      case 'x': imv->loop_input = false;           break;
      case 'l': imv->list_files_at_exit = true;    break;
      case 'n': imv->starting_path = optarg;       break;
      case 'e': imv->font_name = strdup(optarg);   break;
      case 'h':
        fprintf(stdout,
        "imv %s\n"
        "See manual for usage information.\n"
        "\n"
        "Legal:\n"
        "This program is free software; you can redistribute it and/or\n"
        "modify it under the terms of the GNU General Public License\n"
        "as published by the Free Software Foundation; either version 2\n"
        "of the License, or (at your option) any later version.\n"
        "\n"
        "This software uses the FreeImage open source image library.\n"
        "See http://freeimage.sourceforge.net for details.\n"
        "FreeImage is used under the GNU GPLv2.\n"
        , IMV_VERSION);
        imv->quit = true;
        return true;
      case 'b':
        if(!parse_bg(imv, optarg)) {
          return false;
        }
        break;
      case 't':
        if(!parse_slideshow_duration(imv, optarg)) {
          return false;
        }
        break;
      case '?':
        fprintf(stderr, "Unknown argument '%c'. Aborting.\n", optopt);
        return false;
    }
  }

  argc -= optind;
  argv += optind;

  /* if no paths are given as args, expect them from stdin */
  if(argc == 0) {
    imv->paths_from_stdin = true;
  } else {
    /* otherwise, add the paths */
    bool data_from_stdin = false;
    for(int i = 0; i < argc; ++i) {

      /* Special case: '-' denotes reading image data from stdin */
      if(!strcmp("-", argv[i])) {
        if(imv->paths_from_stdin) {
          fprintf(stderr, "Can't read paths AND image data from stdin. Aborting.\n");
          return false;
        } else if(data_from_stdin) {
          fprintf(stderr, "Can't read image data from stdin twice. Aborting.\n");
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
  if(imv->quit)
    return 0;

  if(!setup_window(imv))
    return 1;

  /* if loading paths from stdin, kick off a thread to do that - we'll receive
   * events back via SDL */
  if(imv->paths_from_stdin) {
    SDL_Thread *thread;
    thread = SDL_CreateThread(load_paths_from_stdin, "load_paths_from_stdin", imv);
    SDL_DetachThread(thread);
  }

  if(imv->starting_path) {
    int index = imv_navigator_find_path(imv->navigator, imv->starting_path);
    if(index == -1) {
      index = (int) strtol(imv->starting_path, NULL, 10);
      index -= 1; /* input is 1-indexed, internally we're 0 indexed */
      if(errno == EINVAL) {
        index = -1;
      }
    }

    if(index >= 0) {
      imv_navigator_select_str(imv->navigator, index);
    } else {
      fprintf(stderr, "Invalid starting image: %s\n", imv->starting_path);
    }
  }

  /* cache current image's dimensions */
  imv->current_image.width = 0;
  imv->current_image.height = 0;

  /* time keeping */
  unsigned int last_time = SDL_GetTicks();
  unsigned int current_time;

  while(!imv->quit) {

    SDL_Event e;
    while(!imv->quit && SDL_PollEvent(&e)) {
      handle_event(imv, &e);
    }

    /* if we're quitting, don't bother drawing any more images */
    if(imv->quit) {
      break;
    }

    /* Check if navigator wrapped around paths lists */
    if(!imv->loop_input && imv_navigator_wrapped(imv->navigator)) {
      break;
    }

    /* if the user has changed image, start loading the new one */
    if(imv_navigator_poll_changed(imv->navigator)) {
      const char *current_path = imv_navigator_selection(imv->navigator);
      if(!current_path) {
        if(!imv->paths_from_stdin) {
          fprintf(stderr, "No input files left. Exiting.\n");
          imv->quit = true;
        }
        continue;
      }

      char title[1024];
      const size_t index_cur = imv_navigator_index(imv->navigator);
      const size_t index_len = imv_navigator_length(imv->navigator);
      snprintf(title, sizeof(title), "imv - [%zu/%zu] [LOADING] %s [%s]",
          index_cur + 1, index_len, current_path,
          scaling_label[imv->scaling_mode]);
      imv_viewport_set_title(imv->view, title);

      imv_loader_load(imv->loader, current_path,
          imv->stdin_image_data, imv->stdin_image_data_len);
      imv->view->playing = true;
    }

    if(imv->need_rescale) {
      int ww, wh;
      SDL_GetWindowSize(imv->window, &ww, &wh);

      imv->need_rescale = false;
      if(imv->scaling_mode == SCALING_NONE ||
          (imv->scaling_mode == SCALING_DOWN
           && ww > imv->current_image.width
           && wh > imv->current_image.height)) {
        imv_viewport_scale_to_actual(imv->view, imv->image);
      } else {
        imv_viewport_scale_to_window(imv->view, imv->image);
      }
    }

    /* tell the loader time has passed (for gifs) */
    current_time = SDL_GetTicks();

    /* if we're playing an animated gif, tell the loader how much time has
     * passed */
    if(imv->view->playing) {
      unsigned int dt = current_time - last_time;
      /* We cap the delta-time to 100 ms so that if imv is asleep for several
       * seconds or more (e.g. suspended), upon waking up it doesn't try to
       * catch up all the time it missed by playing through the gif quickly. */
      if(dt > 100) {
        dt = 100;
      }
      imv_loader_time_passed(imv->loader, dt / 1000.0);
    }

    /* handle slideshow */
    if(imv->slideshow_image_duration != 0) {
      unsigned int dt = current_time - last_time;

      imv->slideshow_time_elapsed += dt;
      imv->need_redraw = true; /* need to update display */
      if(imv->slideshow_time_elapsed >= imv->slideshow_image_duration) {
        imv_navigator_select_rel(imv->navigator, 1);
        imv->slideshow_time_elapsed = 0;
      }
    }

    last_time = current_time;


    /* check if the viewport needs a redraw */
    if(imv_viewport_needs_redraw(imv->view)) {
      imv->need_redraw = true;
    }

    if(imv->need_redraw) {
      render_window(imv);
      SDL_RenderPresent(imv->renderer);
    }

    /* sleep until we have something to do */
    unsigned int timeout = 1000; /* sleep up to a second */

    /* if we need to display the next frame of an animation soon we should
      * limit our sleep until the next frame is due */
    const double next_frame_in = imv_loader_time_left(imv->loader);
    if(next_frame_in > 0.0) {
      timeout = (unsigned int)(next_frame_in * 1000.0);
    }

    /* go to sleep until an input event, etc. or the timeout expires */
    SDL_WaitEventTimeout(NULL, timeout);
  }

  if(imv->list_files_at_exit) {
    for(size_t i = 0; i < imv_navigator_length(imv->navigator); ++i)
      puts(imv_navigator_at(imv->navigator, i));
  }

  return 0;
}

static bool setup_window(struct imv *imv)
{
  if(SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL Failed to Init: %s\n", SDL_GetError());
    return false;
  }

  /* register custom events */
  imv->events.NEW_IMAGE = SDL_RegisterEvents(1);
  imv->events.BAD_IMAGE = SDL_RegisterEvents(1);
  imv->events.NEW_PATH = SDL_RegisterEvents(1);

  /* tell the loader which event ids it should use */
  imv_loader_set_event_types(imv->loader,
      imv->events.NEW_IMAGE,
      imv->events.BAD_IMAGE);

  imv->sdl_init = true;

  /* width and height arbitrarily chosen. Perhaps there's a smarter way to
   * set this */
  const int width = 1280;
  const int height = 720;

  imv->window = SDL_CreateWindow(
        "imv",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_RESIZABLE);

  if(!imv->window) {
    fprintf(stderr, "SDL Failed to create window: %s\n", SDL_GetError());
    return false;
  }

  /* we'll use SDL's built-in renderer, hardware accelerated if possible */
  imv->renderer = SDL_CreateRenderer(imv->window, -1, 0);
  if(!imv->renderer) {
    fprintf(stderr, "SDL Failed to create renderer: %s\n", SDL_GetError());
    return false;
  }

  /* use the appropriate resampling method */
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
    imv->nearest_neighbour ? "0" : "1");

  /* construct a chequered background image */
  if(imv->background_type == BACKGROUND_CHEQUERED) {
    imv->background_image = create_chequered(imv->renderer);
  }

  /* set up the required fonts and surfaces for displaying the overlay */
  TTF_Init();
  imv->ttf_init = true;
  imv->font = load_font(imv->font_name);
  if(!imv->font) {
    fprintf(stderr, "Error loading font: %s\n", TTF_GetError());
    return false;
  }

  imv->image = imv_image_create(imv->renderer);
  imv->view = imv_viewport_create(imv->window);

  /* put us in fullscren mode to begin with if requested */
  if(imv->fullscreen) {
    imv_viewport_toggle_fullscreen(imv->view);
  }

  /* start outside of command mode */
  SDL_StopTextInput();

  return true;
}

static void handle_event(struct imv *imv, SDL_Event *event)
{
  const int command_buffer_len = 1024;

  if(event->type == imv->events.NEW_IMAGE) {
    /* new image to display */
    struct imv_bitmap *bmp = event->user.data1;
    imv_image_set_bitmap(imv->image, bmp);
    imv->current_image.width = bmp->width;
    imv->current_image.height = bmp->height;
    imv_bitmap_free(bmp);
    imv->need_redraw = true;
    imv->need_rescale |= event->user.code;
    return;
  } else if(event->type == imv->events.BAD_IMAGE) {
    /* an image failed to load, remove it from our image list */
    char *err_path = event->user.data1;
    imv_navigator_remove(imv->navigator, err_path);

    /* special case: the image came from stdin */
    if(strcmp(err_path, "-") == 0) {
      if(imv->stdin_image_data) {
        free(imv->stdin_image_data);
        imv->stdin_image_data = NULL;
        imv->stdin_image_data_len = 0;
      }
      fprintf(stderr, "Failed to load image from stdin.\n");
    }
    free(err_path);
  } else if(event->type == imv->events.NEW_PATH) {
    /* received a new path from the stdin reading thread */
    imv_add_path(imv, event->user.data1);
    free(event->user.data1);
  }

  switch(event->type) {
    case SDL_QUIT:
      imv_command_exec(imv->commands, "quit", imv);
      break;

    case SDL_TEXTINPUT:
      strncat(imv->input_buffer, event->text.text, command_buffer_len - 1);
      imv->need_redraw = true;
      break;

    case SDL_KEYDOWN:
      SDL_ShowCursor(SDL_DISABLE);

      if(imv->input_buffer) {
        /* in command mode, update the buffer */
        if(event->key.keysym.sym == SDLK_ESCAPE) {
          SDL_StopTextInput();
          free(imv->input_buffer);
          imv->input_buffer = NULL;
          imv->need_redraw = true;
        } else if(event->key.keysym.sym == SDLK_RETURN) {
          imv_command_exec(imv->commands, imv->input_buffer, imv);
          SDL_StopTextInput();
          free(imv->input_buffer);
          imv->input_buffer = NULL;
          imv->need_redraw = true;
        } else if(event->key.keysym.sym == SDLK_BACKSPACE) {
          const size_t len = strlen(imv->input_buffer);
          if(len > 0) {
            imv->input_buffer[len - 1] = '\0';
            imv->need_redraw = true;
          }
        }

        return;
      }

      switch (event->key.keysym.sym) {
        case SDLK_SEMICOLON:
          if(event->key.keysym.mod & KMOD_SHIFT) {
            SDL_StartTextInput();
            imv->input_buffer = malloc(command_buffer_len);
            imv->input_buffer[0] = '\0';
            imv->need_redraw = true;
          }
          break;
        default: { /* braces to allow const char *cmd definition */
          const char *cmd = imv_bind_handle_event(imv->binds, event);
          if(cmd) {
            imv_command_exec(imv->commands, cmd, imv);
          }
        }
      }
      break;
    case SDL_MOUSEWHEEL:
      imv_viewport_zoom(imv->view, imv->image, IMV_ZOOM_MOUSE, event->wheel.y);
      SDL_ShowCursor(SDL_ENABLE);
      break;
    case SDL_MOUSEMOTION:
      if(event->motion.state & SDL_BUTTON_LMASK) {
        imv_viewport_move(imv->view, event->motion.xrel, event->motion.yrel, imv->image);
      }
      SDL_ShowCursor(SDL_ENABLE);
      break;
    case SDL_WINDOWEVENT:
      /* For some reason SDL passes events to us that occurred before we
       * gained focus, and passes them *after* the focus gained event.
       * Due to behavioural quirks from such events, whenever we gain focus
       * we have to clear the event queue. It's hacky, but works without
       * any visible side effects.
       */
      if(event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
      }

      imv_viewport_update(imv->view, imv->image);
      break;
  }
}

static void render_window(struct imv *imv)
{
  char title[1024];
  int ww, wh;
  SDL_GetWindowSize(imv->window, &ww, &wh);

  /* update window title */
  const char *current_path = imv_navigator_selection(imv->navigator);
  const size_t index_cur = imv_navigator_index(imv->navigator);
  const size_t index_len = imv_navigator_length(imv->navigator);
  int len = snprintf(title, sizeof(title), "imv - [%zu/%zu] [%ix%i] [%.2f%%] %s [%s]",
      index_cur + 1, index_len,
      imv_image_width(imv->image), imv_image_height(imv->image),
      100.0 * imv->view->scale,
      current_path, scaling_label[imv->scaling_mode]);
  if(imv->slideshow_image_duration >= 1000) {
    len += snprintf(title + len, sizeof(title) - len, "[%lu/%lus]",
        imv->slideshow_time_elapsed / 1000 + 1, imv->slideshow_image_duration / 1000);
  }
  imv_viewport_set_title(imv->view, title);

  /* first we draw the background */
  if(imv->background_type == BACKGROUND_SOLID) {
    /* solid background */
    SDL_SetRenderDrawColor(imv->renderer,
        imv->background_color.r,
        imv->background_color.g,
        imv->background_color.b,
        255);
    SDL_RenderClear(imv->renderer);
  } else {
    /* chequered background */
    int img_w, img_h;
    SDL_QueryTexture(imv->background_image, NULL, NULL, &img_w, &img_h);
    /* tile the image so it fills the window */
    for(int y = 0; y < wh; y += img_h) {
      for(int x = 0; x < ww; x += img_w) {
        SDL_Rect dst_rect = {x,y,img_w,img_h};
        SDL_RenderCopy(imv->renderer, imv->background_image, NULL, &dst_rect);
      }
    }
  }

  /* draw our actual image */
  imv_image_draw(imv->image, imv->view->x, imv->view->y, imv->view->scale);

  /* if the overlay needs to be drawn, draw that too */
  if(imv->overlay_enabled && imv->font) {
    SDL_Color fg = {255,255,255,255};
    SDL_Color bg = {0,0,0,160};
    imv_printf(imv->renderer, imv->font, 0, 0, &fg, &bg, "%s",
        title + strlen("imv - "));
  }

  /* draw command entry bar if needed */
  if(imv->input_buffer && imv->font) {
    SDL_Color fg = {255,255,255,255};
    SDL_Color bg = {0,0,0,160};
    imv_printf(imv->renderer,
        imv->font,
        0, wh - TTF_FontHeight(imv->font),
        &fg, &bg,
        ":%s", imv->input_buffer);
  }

  /* redraw complete, unset the flag */
  imv->need_redraw = false;
}

static char *get_config_path(void)
{
  const char *config_paths[] = {
    "$HOME/.imv_config",
    "$HOME/.imv/config",
    "$XDG_CONFIG_HOME/imv/config",
    "/usr/local/etc/imv_config",
    "/etc/imv_config",
  };

  for(size_t i = 0; i < sizeof(config_paths) / sizeof(char*); ++i) {
    wordexp_t word;
    if(wordexp(config_paths[i], &word, 0) == 0) {
      char *path = strdup(word.we_wordv[0]);
      wordfree(&word);

      if(!path || access(path, R_OK) == -1) {
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
    const char *err = add_bind(imv, name, value);
    if (err) {
      fprintf(stderr, "Config error: %s\n", err);
      return 0;
    }
    return 1;
  }

  if (!strcmp(section, "aliases")) {
    imv_command_alias(imv->commands, name, value);
    return 1;
  }

  if (!strcmp(section, "options")) {

    if(!strcmp(name, "fullscreen")) {
      imv->fullscreen = parse_bool(value);
      return 1;
    }

    if(!strcmp(name, "overlay")) {
      imv->overlay_enabled = parse_bool(value);
      return 1;
    }

    if(!strcmp(name, "upscaling_method")) {
      imv->nearest_neighbour = !strcmp(value, "nearest_neighbour");
      return 1;
    }

    if(!strcmp(name, "recursive")) {
      imv->recursive_load = parse_bool(value);
      return 1;
    }

    if(!strcmp(name, "loop_input")) {
      imv->loop_input = parse_bool(value);
      return 1;
    }

    if(!strcmp(name, "list_files_at_exit")) {
      imv->list_files_at_exit = parse_bool(value);
      return 1;
    }

    if(!strcmp(name, "scaling_mode")) {
      if(!strcmp(value, "none")) {
        imv->scaling_mode = SCALING_NONE;
      } else if(!strcmp(value, "shrink")) {
        imv->scaling_mode = SCALING_DOWN;
      } else if(!strcmp(value, "full")) {
        imv->scaling_mode = SCALING_FULL;
      }
      return 1;
    }

    if(!strcmp(name, "background")) {
      if(!parse_bg(imv, value)) {
        return false;
      }
      return 1;
    }

    if(!strcmp(name, "slideshow_duration")) {
      if(!parse_slideshow_duration(imv, value)) {
        return false;
      }
      return 1;
    }

    if(!strcmp(name, "overlay_font")) {
      free(imv->font_name);
      imv->font_name = strdup(value);
      return 1;
    }

    if(!strcmp(name, "suppress_default_binds")) {
      const bool suppress_default_binds = parse_bool(value);
      if(suppress_default_binds) {
        /* clear out any default binds if requested */
        imv_binds_clear(imv->binds);
      }
      return 1;
    }

    /* No matches so far */
    fprintf(stderr, "Ignoring unknown option: %s\n", name);
    return 1;
  }
  return 0;
}

bool imv_load_config(struct imv *imv)
{
  const char *path = get_config_path();
  if(!path) {
    /* no config, no problem - we have defaults */
    return true;
  }

  const int err = ini_parse(path, handle_ini_value, imv);
  if (err == -1) {
    fprintf(stderr, "Unable to open config file: %s\n", path);
    return false;
  } else if (err > 0) {
    fprintf(stderr, "Error in config file: %s:%d\n", path, err);
    return false;
  }
  return true;
}

void command_quit(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  imv->quit = true;
}

void command_pan(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  if(args->len != 3) {
    return;
  }

  long int x = strtol(args->items[1], NULL, 10);
  long int y = strtol(args->items[2], NULL, 10);

  imv_viewport_move(imv->view, x, y, imv->image);
}

void command_select_rel(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  if(args->len != 2) {
    return;
  }

  long int index = strtol(args->items[1], NULL, 10);
  imv_navigator_select_rel(imv->navigator, index);

  imv->slideshow_time_elapsed = 0;
}

void command_select_abs(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  if(args->len != 2) {
    return;
  }

  long int index = strtol(args->items[1], NULL, 10);
  imv_navigator_select_abs(imv->navigator, index);

  imv->slideshow_time_elapsed = 0;
}

void command_zoom(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  if(args->len == 2) {
    const char *str = args->items[1];
    if(!strcmp(str, "actual")) {
      imv_viewport_scale_to_actual(imv->view, imv->image);
    } else {
      long int amount = strtol(args->items[1], NULL, 10);
      imv_viewport_zoom(imv->view, imv->image, IMV_ZOOM_KEYBOARD, amount);
    }
  }
}

void command_remove(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  char* path = strdup(imv_navigator_selection(imv->navigator));
  imv_navigator_remove(imv->navigator, path);
  free(path);

  imv->slideshow_time_elapsed = 0;
}

void command_fullscreen(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  imv_viewport_toggle_fullscreen(imv->view);
}

void command_overlay(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  imv->overlay_enabled = !imv->overlay_enabled;
  imv->need_redraw = true;
}

void command_exec(struct list *args, const char *argstr, void *data)
{
  (void)args;
  struct imv *imv = data;
  setenv("imv_current_file", imv_navigator_selection(imv->navigator), 1);
  system(argstr);
}

void command_center(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  imv_viewport_center(imv->view, imv->image);
}

void command_reset(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  imv->need_rescale = true;
  imv->need_redraw = true;
}

void command_next_frame(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  imv_loader_load_next_frame(imv->loader);
}

void command_toggle_playing(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;
  imv_viewport_toggle_playing(imv->view);
}

void command_set_scaling_mode(struct list *args, const char *argstr, void *data)
{
  (void)args;
  (void)argstr;
  struct imv *imv = data;

  if(args->len != 2) {
    return;
  }

  const char *mode = args->items[1];

  if(!strcmp(mode, "next")) {
    imv->scaling_mode++;
    imv->scaling_mode %= SCALING_MODE_COUNT;
  } else if(!strcmp(mode, "none")) {
    imv->scaling_mode = SCALING_NONE;
  } else if(!strcmp(mode, "shrink")) {
    imv->scaling_mode = SCALING_DOWN;
  } else if(!strcmp(mode, "full")) {
    imv->scaling_mode = SCALING_FULL;
  } else {
    /* no changes, don't bother to redraw */
    return;
  }

  imv->need_rescale = true;
  imv->need_redraw = true;
}

void command_set_slideshow_duration(struct list *args, const char *argstr, void *data)
{
  (void)argstr;
  struct imv *imv = data;
  if(args->len == 2) {
    long int delta = 1000 * strtol(args->items[1], NULL, 10);

    /* Ensure we can't go below 0 */
    if(delta < 0 && (size_t)labs(delta) > imv->slideshow_image_duration) {
      imv->slideshow_image_duration = 0;
    } else {
      imv->slideshow_image_duration += delta;
    }

    imv->need_redraw = true;
  }
}

/* vim:set ts=2 sts=2 sw=2 et: */
