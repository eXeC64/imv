/* Copyright (c) 2017 imv authors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "imv.h"

#include <getopt.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "commands.h"
#include "list.h"
#include "loader.h"
#include "texture.h"
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
  "best fit",
  "perfect fit"
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
  bool cycle_input;
  bool list_at_exit;
  enum scaling_mode scaling_mode;
  enum background_type background_type;
  struct { unsigned char r, g, b; } background_color;
  unsigned long slideshow_image_duration;
  unsigned long slideshow_time_elapsed;
  char *font_name;
  struct imv_navigator *navigator;
  struct imv_loader *loader;
  struct imv_commands *commands;
  struct imv_texture *texture;
  struct imv_viewport *view;
  char *input_buffer;
  char *starting_path;

  SDL_Window *window;
  SDL_Renderer *renderer;
  TTF_Font *font;
  SDL_Texture *background_texture;
  bool sdl_init;
  bool ttf_init;
};

static bool setup_window(struct imv *imv);
static void handle_event(struct imv *imv, SDL_Event *event);
static void render_window(struct imv *imv);

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
  imv->scaling_mode = SCALING_NONE;
  imv->cycle_input = true;
  imv->list_at_exit = false;
  imv->background_color.r = imv->background_color.g = imv->background_color.b = 0;
  imv->slideshow_image_duration = 0;
  imv->slideshow_time_elapsed = 0;
  imv->font_name = strdup("Monospace:24");
  imv->navigator = imv_navigator_create();
  imv->loader = imv_loader_create();
  imv->commands = imv_commands_create();
  imv->input_buffer = NULL;
  imv->starting_path = NULL;
  imv->window = NULL;
  imv->renderer = NULL;
  imv->font = NULL;
  imv->background_texture = NULL;
  imv->sdl_init = false;
  imv->ttf_init = false;
  return imv;
}

void imv_free(struct imv *imv)
{
  free(imv->font_name);
  imv_navigator_free(imv->navigator);
  imv_loader_free(imv->loader);
  imv_commands_free(imv->commands);
  if(imv->input_buffer) {
    free(imv->input_buffer);
  }
  if(imv->renderer) {
    SDL_DestroyRenderer(imv->renderer);
  }
  if(imv->window) {
    SDL_DestroyWindow(imv->window);
  }
  if(imv->background_texture) {
    SDL_DestroyTexture(imv->background_texture);
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

bool imv_parse_args(struct imv *imv, int argc, char **argv)
{
  /* Do not print getopt errors */
  opterr = 0;

  char *argp, *ep = *argv;
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
      case 'x': imv->cycle_input = false;          break;
      case 'l': imv->list_at_exit = true;          break;
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
        if(strcmp("checks", optarg) == 0) {
          imv->background_type = BACKGROUND_CHEQUERED;
        } else {
          imv->background_type = BACKGROUND_SOLID;
          argp = (*optarg == '#') ? optarg + 1 : optarg;
          uint32_t n = strtoul(argp, &ep, 16);
          if(*ep != '\0' || ep - argp != 6 || n > 0xFFFFFF) {
            fprintf(stderr, "Invalid hex color: '%s'\n", optarg);
            return false;
          }
          imv->background_color.b = n & 0xFF;
          imv->background_color.g = (n >> 8) & 0xFF;
          imv->background_color.r = (n >> 16);
        }
        break;
      case 't':
        imv->slideshow_image_duration = strtoul(optarg, &argp, 10);
        imv->slideshow_image_duration *= 1000;
        if (*argp == '.') {
          long delay = strtoul(++argp, &ep, 10);
          for (int i = 3 - (ep - argp); i; i--) {
            delay *= 10;
          }
          if (delay < 1000) {
            imv->slideshow_image_duration += delay;
          } else {
            imv->slideshow_image_duration = ULONG_MAX;
          }
        }
        if (imv->slideshow_image_duration == ULONG_MAX) {
          fprintf(stderr, "Wrong slideshow delay '%s'. Aborting.\n", optarg);
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

  /* TODO parse paths ? */
  return true;
}

void imv_add_path(struct imv *imv, const char *path)
{
  imv_navigator_add(imv->navigator, path, imv->recursive_load);
}

bool imv_run(struct imv *imv)
{
  if(!setup_window(imv))
    return false;

  imv->quit = 0;

  /* cache current image's dimensions */
  int iw = 0;
  int ih = 0;

  while(!imv->quit) {

    SDL_Event e;
    while(!imv->quit && SDL_PollEvent(&e)) {
      handle_event(imv, &e);
    }

    /* if we're quitting, don't bother drawing any more images */
    if(imv->quit) {
      break;
    }

    /* check if an image failed to load, if so, remove it from our image list */
    char *err_path = imv_loader_get_error(imv->loader);
    if(err_path) {
      imv_navigator_remove(imv->navigator, err_path);

      /* special case: the image came from stdin */
      /* if (strncmp(err_path, "-", 2) == 0) { */
      /*   free(stdin_buffer); */
      /*   stdin_buffer_size = 0; */
      /*   if (stdin_error != 0) { */
      /*     errno = stdin_error; */
      /*     perror("Failed to load image from standard input"); */
      /*     errno = 0; */
      /*   } */
      /* } */
      free(err_path);
    }

    /* Check if navigator wrapped around paths lists */
    if(!imv->cycle_input && imv_navigator_wrapped(imv->navigator)) {
      break;
    }

    /* if the user has changed image, start loading the new one */
    if(imv_navigator_poll_changed(imv->navigator)) {
      const char *current_path = imv_navigator_selection(imv->navigator);
      if(!current_path) {
        /* if(!imv->stdin_list) { */
          fprintf(stderr, "No input files left. Exiting.\n");
          imv->quit = 1;
        /* } */
        continue;
      }

      char title[1024];
      snprintf(title, sizeof(title), "imv - [%i/%i] [LOADING] %s [%s]",
          imv->navigator->cur_path + 1, imv->navigator->num_paths, current_path,
          scaling_label[imv->scaling_mode]);
      imv_viewport_set_title(imv->view, title);

      imv_loader_load(imv->loader, current_path, "", 0 /*stdin_buffer, stdin_buffer_size*/);
      imv->view->playing = 1;
    }


    /* check if a new image is available to display */
    FIBITMAP *bmp;
    int is_new_image;
    if(imv_loader_get_image(imv->loader, &bmp, &is_new_image)) {
      imv_texture_set_image(imv->texture, bmp);
      iw = FreeImage_GetWidth(bmp);
      ih = FreeImage_GetHeight(bmp);
      FreeImage_Unload(bmp);
      imv->need_redraw = 1;
      imv->need_rescale += is_new_image;
    }

    if(imv->need_rescale) {
      int ww, wh;
      SDL_GetWindowSize(imv->window, &ww, &wh);

      imv->need_rescale = 0;
      if(imv->scaling_mode == SCALING_NONE ||
          (imv->scaling_mode == SCALING_DOWN && ww > iw && wh > ih)) {
        imv_viewport_scale_to_actual(imv->view, imv->texture);
      } else {
        imv_viewport_scale_to_window(imv->view, imv->texture);
      }
    }

    if(imv->need_redraw) {
      render_window(imv);
      SDL_RenderPresent(imv->renderer);
    }

    /* sleep a little bit so we don't waste CPU time */
    SDL_Delay(10);
  }

  return 0;
}

static bool setup_window(struct imv *imv)
{
  if(SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL Failed to Init: %s\n", SDL_GetError());
    return false;
  }
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

  /* construct a chequered background texture */
  if(imv->background_type == BACKGROUND_CHEQUERED) {
    imv->background_texture = create_chequered(imv->renderer);
  }

  /* set up the required fonts and surfaces for displaying the overlay */
  TTF_Init();
  imv->ttf_init = true;
  imv->font = load_font(imv->font_name);
  if(!imv->font) {
    fprintf(stderr, "Error loading font: %s\n", TTF_GetError());
    return false;
  }

  imv->texture = imv_texture_create(imv->renderer);
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
  switch(event->type) {
    case SDL_QUIT:
      imv_command_exec(imv->commands, "quit", NULL);
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
          imv_command_exec(imv->commands, imv->input_buffer, NULL);
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
        case SDLK_q:
          imv_command_exec(imv->commands, "quit", NULL);
          break;
        case SDLK_LEFTBRACKET:
        case SDLK_LEFT:
          imv_command_exec(imv->commands, "select_rel -1", NULL);
          break;
        case SDLK_RIGHTBRACKET:
        case SDLK_RIGHT:
          imv_command_exec(imv->commands, "select_rel 1", NULL);
          break;
        case SDLK_EQUALS:
        case SDLK_PLUS:
        case SDLK_i:
        case SDLK_UP:
          imv_viewport_zoom(imv->view, imv->texture, IMV_ZOOM_KEYBOARD, 1);
          break;
        case SDLK_MINUS:
        case SDLK_o:
        case SDLK_DOWN:
          imv_viewport_zoom(imv->view, imv->texture, IMV_ZOOM_KEYBOARD, -1);
          break;
        case SDLK_s:
          if(!event->key.repeat) {
            imv->scaling_mode++;
            imv->scaling_mode %= SCALING_MODE_COUNT;
          }
        /* FALLTHROUGH */
        case SDLK_r:
          if(!event->key.repeat) {
            imv->need_rescale = true;
            imv->need_redraw = true;
          }
          break;
        case SDLK_a:
          if(!event->key.repeat) {
            imv_viewport_scale_to_actual(imv->view, imv->texture);
          }
          break;
        case SDLK_c:
          if(!event->key.repeat) {
            imv_viewport_center(imv->view, imv->texture);
          }
          break;
        case SDLK_j:
          imv_command_exec(imv->commands, "pan 0 -50", NULL);
          break;
        case SDLK_k:
          imv_command_exec(imv->commands, "pan 0 50", NULL);
          break;
        case SDLK_h:
          imv_command_exec(imv->commands, "pan 50 0", NULL);
          break;
        case SDLK_l:
          imv_command_exec(imv->commands, "pan -50 0", NULL);
          break;
        case SDLK_x:
          if(!event->key.repeat) {
            imv_command_exec(imv->commands, "remove", NULL);
          }
          break;
        case SDLK_f:
          if(!event->key.repeat) {
            imv_command_exec(imv->commands, "fullscreen", NULL);
          }
          break;
        case SDLK_PERIOD:
          imv_loader_load_next_frame(imv->loader);
          break;
        case SDLK_SPACE:
          if(!event->key.repeat) {
            imv_viewport_toggle_playing(imv->view);
          }
          break;
        case SDLK_p:
          if(!event->key.repeat) {
            puts(imv_navigator_selection(imv->navigator));
          }
          break;
        case SDLK_d:
          if(!event->key.repeat) {
            imv_command_exec(imv->commands, "overlay", NULL);
          }
          break;
        case SDLK_t:
          if(event->key.keysym.mod & (KMOD_SHIFT|KMOD_CAPS)) {
            if(imv->slideshow_image_duration >= 1000) {
              imv->slideshow_image_duration -= 1000;
            }
          } else {
            imv->slideshow_image_duration += 1000;
          }
          imv->need_redraw = true;
          break;
      }
      break;
    case SDL_MOUSEWHEEL:
      imv_viewport_zoom(imv->view, imv->texture, IMV_ZOOM_MOUSE, event->wheel.y);
      SDL_ShowCursor(SDL_ENABLE);
      break;
    case SDL_MOUSEMOTION:
      if(event->motion.state & SDL_BUTTON_LMASK) {
        imv_viewport_move(imv->view, event->motion.xrel, event->motion.yrel);
      }
      SDL_ShowCursor(SDL_ENABLE);
      break;
    case SDL_WINDOWEVENT:
      imv_viewport_update(imv->view, imv->texture);
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
  int len = snprintf(title, sizeof(title), "imv - [%i/%i] [%ix%i] [%.2f%%] %s [%s]",
      imv->navigator->cur_path + 1, imv->navigator->num_paths, imv->texture->width, imv->texture->height,
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
    SDL_QueryTexture(imv->background_texture, NULL, NULL, &img_w, &img_h);
    /* tile the texture so it fills the window */
    for(int y = 0; y < wh; y += img_h) {
      for(int x = 0; x < ww; x += img_w) {
        SDL_Rect dst_rect = {x,y,img_w,img_h};
        SDL_RenderCopy(imv->renderer, imv->background_texture, NULL, &dst_rect);
      }
    }
  }

  /* draw our actual texture */
  imv_texture_draw(imv->texture, imv->view->x, imv->view->y, imv->view->scale);

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

/* vim:set ts=2 sts=2 sw=2 et: */
