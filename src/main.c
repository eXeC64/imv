/* Copyright (c) 2015 imv authors

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

#include <limits.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <FreeImage.h>
#include <getopt.h>
#include <ctype.h>
#include <poll.h>
#include <errno.h>

#include "loader.h"
#include "texture.h"
#include "navigator.h"
#include "viewport.h"
#include "util.h"

enum scaling_mode {
  NONE,
  DOWN,
  FULL
};

static char *scaling_label[] = {
  "actual size",
  "best fit",
  "perfect fit"
};

struct {
  int fullscreen;
  int stdin_list;
  int recursive;
  int scaling;
  int nearest_neighbour;
  int solid_bg;
  int list;
  unsigned long delay;
  int cycle;
  unsigned char bg_r;
  unsigned char bg_g;
  unsigned char bg_b;
  int overlay;
  const char *start_at;
  const char *font;
} g_options = {
  .fullscreen = 0,
  .stdin_list = 0,
  .recursive = 0,
  .scaling = FULL,
  .nearest_neighbour = 0,
  .solid_bg = 1,
  .list = 0,
  .delay = 0,
  .cycle = 1,
  .bg_r = 0,
  .bg_g = 0,
  .bg_b = 0,
  .overlay = 0,
  .start_at = NULL,
  .font = "Monospace:24",
};

static void print_usage(void)
{
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
}

static void parse_args(int argc, char** argv)
{
  /* Do not print getopt errors */
  opterr = 0;

  char *argp, *ep = *argv;
  int o;

  while((o = getopt(argc, argv, "firasSudxhln:b:e:t:")) != -1) {
    switch(o) {
      case 'f': g_options.fullscreen = 1;   break;
      case 'i':
        g_options.stdin_list = 1;
        fprintf(stderr, "Warning: '-i' is deprecated. No flag is needed.\n");
        break;
      case 'r': g_options.recursive = 1;           break;
      case 'a': g_options.scaling = NONE;          break;
      case 's': g_options.scaling = DOWN;          break;
      case 'S': g_options.scaling = FULL;          break;
      case 'u': g_options.nearest_neighbour = 1;   break;
      case 'd': g_options.overlay = 1;             break;
      case 'x': g_options.cycle = 0;               break;
      case 'h': print_usage(); exit(0);            break;
      case 'l': g_options.list = 1;                break;
      case 'n':
        g_options.start_at = optarg;
        break;
      case 'b':
        if(strcmp("checks", optarg) == 0) {
          g_options.solid_bg = 0;
        } else {
          g_options.solid_bg = 1;
          argp = (*optarg == '#') ? optarg + 1 : optarg;
          uint32_t n = strtoul(argp, &ep, 16);
          if(*ep != '\0' || ep - argp != 6 || n > 0xFFFFFF) {
            fprintf(stderr, "Invalid hex color: '%s'\n", optarg);
            exit(1);
          }
          g_options.bg_b = n & 0xFF;
          g_options.bg_g = (n >> 8) & 0xFF;
          g_options.bg_r = (n >> 16);
        }
        break;
      case 'e':
        g_options.font = optarg;
        break;
      case 't':
        g_options.delay = strtoul(optarg, &argp, 10);
        g_options.delay *= 1000;
        if (*argp == '.') {
          long delay = strtoul(++argp, &ep, 10);
          for (int i = 3 - (ep - argp); i; i--) {
            delay *= 10;
          }
          if (delay < 1000) {
            g_options.delay += delay;
          } else {
            g_options.delay = ULONG_MAX;
          }
        }
        if (g_options.delay == ULONG_MAX) {
          fprintf(stderr, "Wrong slideshow delay '%s'. Aborting.\n", optarg);
          exit(1);
        }
        break;
      case '?':
        fprintf(stderr, "Unknown argument '%c'. Aborting.\n", optopt);
        exit(1);
    }
  }
}

#define _concat(a, n) a##n
#define concat(a, n) _concat(a, n)
#define define_jump(key) \
  case concat(SDLK_KP_, key):\
  case concat(SDLK_, key):\
    if(value <= 0) { \
      value = key; \
    } else { \
      value = value * 10 + key;\
    } \
    delay_msec = 0;\
    need_redraw = 1;\
    break;

int main(int argc, char** argv)
{
  struct imv_navigator nav;
  imv_navigator_init(&nav);

  /* parse any command line options given */
  parse_args(argc, argv);

  argc -= optind;
  argv += optind;

  /* if no names are given, expect them on stdin */
  if(argc == 0) {
    g_options.stdin_list = 1;
  }

  /* if the user asked to load paths from stdin, set up poll(2)ing and read
     the first path */
  struct pollfd rfds[1];
  if(g_options.stdin_list) {
    rfds[0].fd = STDIN_FILENO;
    rfds[0].events = POLLIN;
    fprintf(stderr, "Reading paths from stdin...");

    char buf[PATH_MAX];
    char *stdin_ok;
    while((stdin_ok = fgets(buf, sizeof(buf), stdin)) != NULL) {
      size_t len = strlen(buf);
      if(buf[len-1] == '\n') {
        buf[--len] = 0;
      }
      if(len > 0) {
        if(imv_navigator_add(&nav, buf, g_options.recursive) != 0) {
          imv_navigator_destroy(&nav);
          exit(1);
        }
        break;
      }
    }
    if(!stdin_ok) {
      fprintf(stderr, " no input!\n");
      return -1;
    }
    fprintf(stderr, "\n");
  }

  void *stdin_buffer = NULL;
  size_t stdin_buffer_size = 0;
  int stdin_error = 0;

  /* handle any image paths given as arguments */
  for(int i = 0; i < argc; ++i) {
    /* special case: '-' is actually an option */
    if(!strcmp("-",argv[i])) {
      if (stdin_buffer) {
        fprintf(stderr, "Can't read from stdin twice\n");
        exit(1);
      }
      stdin_buffer_size = read_from_stdin(&stdin_buffer);
      if (stdin_buffer_size == 0) {
        perror(NULL);
        continue; /* we can't recover from the freed buffer, just ignore it */
      }
      stdin_error = errno;
      errno = 0; /* clear errno */
    }
    /* add the given path to the list to load */
    if(imv_navigator_add(&nav, argv[i], g_options.recursive) != 0) {
      imv_navigator_destroy(&nav);
      exit(1);
    }
  }

  /* if we weren't given any paths we have nothing to view. exit */
  if(!imv_navigator_selection(&nav)) {
    fprintf(stderr, "No input files. Exiting.\n");
    exit(1);
  }

  /* go to the chosen starting image if possible */
  if(g_options.start_at) {
    int start_index = imv_navigator_find_path(&nav, g_options.start_at);
    if(start_index < 0) {
      start_index = strtol(g_options.start_at,NULL,10) - 1;
    }
    imv_navigator_select_str(&nav, start_index);
  }

  /* we've got something to display, so create an SDL window */
  if(SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL Failed to Init: %s\n", SDL_GetError());
    exit(1);
  }

  /* width and height arbitrarily chosen. Perhaps there's a smarter way to
   * set this */
  const int width = 1280;
  const int height = 720;

  SDL_Window *window = SDL_CreateWindow(
        "imv",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_RESIZABLE);
  if(!window) {
    fprintf(stderr, "SDL Failed to create window: %s\n", SDL_GetError());
    SDL_Quit();
    exit(1);
  }

  /* we'll use SDL's built-in renderer, hardware accelerated if possible */
  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
  if(!renderer) {
    fprintf(stderr, "SDL Failed to create renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    exit(1);
  }

  /* use the appropriate resampling method */
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
    g_options.nearest_neighbour ? "0" : "1");

  /* construct a chequered background texture */
  SDL_Texture *chequered_tex = NULL;
  if(!g_options.solid_bg) {
    chequered_tex = create_chequered(renderer);
  }

  /* set up the required fonts and surfaces for displaying the overlay */
  TTF_Init();
  TTF_Font *font = load_font(g_options.font);
  if(!font) {
    fprintf(stderr, "Error loading font: %s\n", TTF_GetError());
  }

  /* create our main classes on the stack*/
  struct imv_loader ldr;
  imv_init_loader(&ldr);

  struct imv_texture tex;
  imv_init_texture(&tex, renderer);

  struct imv_viewport view;
  imv_init_viewport(&view, window);

  /* put us in fullscren mode to begin with if requested */
  if(g_options.fullscreen) {
    imv_viewport_toggle_fullscreen(&view);
  }

  /* help keeping track of time */
  unsigned int last_time = SDL_GetTicks();
  unsigned int current_time;

  /* keep file change polling rate under control */
  static uint8_t poll_countdown = UINT8_MAX;

  /* do we need to redraw the window? */
  int need_redraw = 1;
  int need_rescale = 0;

  /* keep title buffer around for reuse */
  char title[256];

  /* used to calculate when to skip to the next image in slideshow mode */
  unsigned long delay_msec = 0;

  /* initialize variables holding image dimentions */
  int iw = 0, ih = 0;

  int quit = 0;

  /* Accumulator for "goto" */
  int value = -1;

  while(!quit) {
    /* handle any input/window events sent by SDL */
    SDL_Event e;
    while(!quit && SDL_PollEvent(&e)) {
      switch(e.type) {
        case SDL_QUIT:
          quit = 1;
          break;
        case SDL_KEYDOWN:
          SDL_ShowCursor(SDL_DISABLE);
          switch (e.key.keysym.sym) {
            case SDLK_q:
              quit = 1;
              break;
            case SDLK_LEFTBRACKET:
            case SDLK_LEFT:
              value = -1;
              imv_navigator_select_rel(&nav, -1);
              /* reset slideshow delay */
              delay_msec = 0;
              break;
            case SDLK_RIGHTBRACKET:
            case SDLK_RIGHT:
              value = -1;
              imv_navigator_select_rel(&nav, 1);
              /* reset slideshow delay */
              delay_msec = 0;
              break;
            case SDLK_EQUALS:
            case SDLK_PLUS:
            case SDLK_i:
            case SDLK_UP:
              value = -1;
              imv_viewport_zoom(&view, &tex, IMV_ZOOM_KEYBOARD, 1);
              break;
            case SDLK_MINUS:
            case SDLK_o:
            case SDLK_DOWN:
              value = -1;
              imv_viewport_zoom(&view, &tex, IMV_ZOOM_KEYBOARD, -1);
              break;
            case SDLK_s:
              value = -1;
              if(!e.key.repeat) {
                if((g_options.scaling += 1) > FULL) {
                  g_options.scaling = NONE;
                }
              }
            /* FALLTHROUGH */
            case SDLK_r:
              value = -1;
              if(!e.key.repeat) {
                need_rescale = 1;
                need_redraw = 1;
              }
              break;
            case SDLK_a:
              value = -1;
              if(!e.key.repeat) {
                imv_viewport_scale_to_actual(&view, &tex);
              }
              break;
            case SDLK_c:
              value = -1;
              if(!e.key.repeat) {
                imv_viewport_center(&view, &tex);
              }
              break;
            case SDLK_j:
              value = -1;
              imv_viewport_move(&view, 0, -50);
              break;
            case SDLK_k:
              value = -1;
              imv_viewport_move(&view, 0, 50);
              break;
            case SDLK_h:
              value = -1;
              imv_viewport_move(&view, 50, 0);
              break;
            case SDLK_l:
              value = -1;
              imv_viewport_move(&view, -50, 0);
              break;
            case SDLK_x:
              value = -1;
              if(!e.key.repeat) {
                char* path = strdup(imv_navigator_selection(&nav));
                imv_navigator_remove(&nav, path);

                if (SDL_GetModState() & KMOD_SHIFT) {
                  if (remove(path)) {
                    fprintf(stderr, "Warning: can't remove %s from disk.\n", path);
                  }
                }

                free(path);

                /* reset slideshow delay */
                delay_msec = 0;
              }
              break;
            case SDLK_f:
              value = -1;
              if(!e.key.repeat) {
                imv_viewport_toggle_fullscreen(&view);
              }
              break;
            case SDLK_PERIOD:
              value = -1;
              imv_loader_load_next_frame(&ldr);
              break;
            case SDLK_SPACE:
              value = -1;
              if(!e.key.repeat) {
                imv_viewport_toggle_playing(&view);
              }
              break;
            case SDLK_p:
              value = -1;
              if(!e.key.repeat) {
                puts(imv_navigator_selection(&nav));
              }
              break;
            case SDLK_d:
              value = -1;
              if(!e.key.repeat) {
                g_options.overlay = !g_options.overlay;
                need_redraw = 1;
              }
              break;
            case SDLK_t:
              value = -1;
              if(e.key.keysym.mod & (KMOD_SHIFT|KMOD_CAPS)) {
                if(g_options.delay >= 1000) {
                  g_options.delay -= 1000;
                }
              } else {
                g_options.delay += 1000;
              }
              need_redraw = 1;
              break;
            define_jump(0)
            define_jump(1)
            define_jump(2)
            define_jump(3)
            define_jump(4)
            define_jump(5)
            define_jump(6)
            define_jump(7)
            define_jump(8)
            define_jump(9)
            case SDLK_KP_ENTER:
            case SDLK_RETURN:
              if(value >= 0){
                imv_navigator_select_abs(&nav, value - 1);
              }
              value = -1;
              delay_msec = 0;
              break;
          }
          break;
        case SDL_MOUSEWHEEL:
          imv_viewport_zoom(&view, &tex, IMV_ZOOM_MOUSE, e.wheel.y);
          SDL_ShowCursor(SDL_ENABLE);
          break;
        case SDL_MOUSEMOTION:
          if(e.motion.state & SDL_BUTTON_LMASK) {
            imv_viewport_move(&view, e.motion.xrel, e.motion.yrel);
          }
          SDL_ShowCursor(SDL_ENABLE);
          break;
        case SDL_WINDOWEVENT:
          imv_viewport_update(&view, &tex);
          break;
      }
    }

    /* if we're quitting, don't bother drawing any more images */
    if(quit) {
      break;
    }

    /* check if an image failed to load, if so, remove it from our image list */
    char *err_path = imv_loader_get_error(&ldr);
    if(err_path) {
      imv_navigator_remove(&nav, err_path);
      if (strncmp(err_path, "-", 2) == 0) {
        free(stdin_buffer);
        stdin_buffer_size = 0;
        if (stdin_error != 0) {
          errno = stdin_error;
          perror("Failed to load image from standard input");
          errno = 0;
        }
      }
      free(err_path);
    }

    /* Check if navigator wrapped around paths lists */
    if(!g_options.cycle && imv_navigator_wrapped(&nav)) {
      break;
    }

    /* if the user has changed image, start loading the new one */
    if(imv_navigator_poll_changed(&nav, poll_countdown--)) {
      const char *current_path = imv_navigator_selection(&nav);
      if(!current_path) {
        if(g_options.stdin_list) {
          continue;
        }
        fprintf(stderr, "No input files left. Exiting.\n");
        exit(1);
      }

      snprintf(title, sizeof(title), "imv - [%i/%i] [LOADING] %s [%s]",
          nav.cur_path + 1, nav.num_paths, current_path,
          scaling_label[g_options.scaling]);
      imv_viewport_set_title(&view, title);

      imv_loader_load(&ldr, current_path, stdin_buffer, stdin_buffer_size);
      view.playing = 1;
    }

    /* get window height and width */
    int ww, wh;
    SDL_GetWindowSize(window, &ww, &wh);

    /* check if a new image is available to display */
    FIBITMAP *bmp;
    int is_new_image;
    if(imv_loader_get_image(&ldr, &bmp, &is_new_image)) {
      imv_texture_set_image(&tex, bmp);
      iw = FreeImage_GetWidth(bmp);
      ih = FreeImage_GetHeight(bmp);
      FreeImage_Unload(bmp);
      need_redraw = 1;
      need_rescale += is_new_image;
    }

    if(need_rescale) {
      need_rescale = 0;
      if(g_options.scaling == NONE ||
          (g_options.scaling == DOWN && ww > iw && wh > ih)) {
        imv_viewport_scale_to_actual(&view, &tex);
      } else {
        imv_viewport_scale_to_window(&view, &tex);
      }
    }

    current_time = SDL_GetTicks();

    /* if we're playing an animated gif, tell the loader how much time has
     * passed */
    if(view.playing) {
      unsigned int dt = current_time - last_time;
      /* We cap the delta-time to 100 ms so that if imv is asleep for several
       * seconds or more (e.g. suspended), upon waking up it doesn't try to
       * catch up all the time it missed by playing through the gif quickly. */
      if(dt > 100) {
        dt = 100;
      }
      imv_loader_time_passed(&ldr, dt / 1000.0);
    }

    /* handle slideshow */
    if(g_options.delay) {
      unsigned int dt = current_time - last_time;

      delay_msec += dt;
      need_redraw = 1;
      if(delay_msec >= g_options.delay) {
        imv_navigator_select_rel(&nav, 1);
        delay_msec = 0;
      }
    }

    last_time = current_time;

    /* check if the viewport needs a redraw */
    if(imv_viewport_needs_redraw(&view)) {
      need_redraw = 1;
    }

    /* only redraw when something's changed */
    if(need_redraw) {
      /* update window title */
      int len;
      const char *current_path = imv_navigator_selection(&nav);
      len = snprintf(title, sizeof(title), "imv - [%i/%i] [%ix%i] [%.2f%%] %s [%s]",
          nav.cur_path + 1, nav.num_paths, tex.width, tex.height,
          100.0 * view.scale,
          current_path, scaling_label[g_options.scaling]);
      if(g_options.delay >= 1000) {
        len += snprintf(title + len, sizeof(title) - len, "[%lu/%lus]",
            delay_msec / 1000 + 1, g_options.delay / 1000);
      }
      imv_viewport_set_title(&view, title);

      /* first we draw the background */
      if(g_options.solid_bg) {
        /* solid background */
        SDL_SetRenderDrawColor(renderer,
            g_options.bg_r, g_options.bg_g, g_options.bg_b, 255);
        SDL_RenderClear(renderer);
      } else {
        /* chequered background */
        int img_w, img_h;
        SDL_QueryTexture(chequered_tex, NULL, NULL, &img_w, &img_h);
        /* tile the texture so it fills the window */
        for(int y = 0; y < wh; y += img_h) {
          for(int x = 0; x < ww; x += img_w) {
            SDL_Rect dst_rect = {x,y,img_w,img_h};
            SDL_RenderCopy(renderer, chequered_tex, NULL, &dst_rect);
          }
        }
      }

      /* draw our actual texture */
      imv_texture_draw(&tex, view.x, view.y, view.scale);

      /* if the overlay needs to be drawn, draw that too */
      if(g_options.overlay && font) {
        SDL_Color fg = {255,255,255,255};
        SDL_Color bg = {0,0,0,160};
        imv_printf(renderer, font, 0, 0, &fg, &bg, "%s",
            title + strlen("imv - "));
      }
      if(font && value >= 0) {
        printf("Drawing\n");
        SDL_Color fg = {255,255,255,255};
        SDL_Color bg = {0,0,0,160};
        // x = ww - strlen(txt) * font size
        // y = font size + 10% (padding)
        int size = TTF_FontHeight(font);
        char txt[50];
        sprintf(txt, "Jump to image %d", value);
        imv_printf(renderer, font,
                   0,
                   size + (0.1 * size), &fg, &bg,
                   "Jump to image %d", value);
      }

      /* redraw complete, unset the flag */
      need_redraw = 0;

      /* reset poll countdown timer */
      poll_countdown = UINT8_MAX;

      /* tell SDL to show the newly drawn frame */
      SDL_RenderPresent(renderer);
    }

    /* sleep a little bit so we don't waste CPU time */
    if(g_options.stdin_list) {
      if(poll(rfds, 1, 10) != 1 || rfds[0].revents & (POLLERR|POLLNVAL)) {
        fprintf(stderr, "error polling stdin");
        return 1;
      }
      if(rfds[0].revents & (POLLIN|POLLHUP)) {
        char buf[PATH_MAX];
        if(fgets(buf, sizeof(buf), stdin) == NULL && ferror(stdin)) {
          clearerr(stdin);
          continue;
        }
        if(feof(stdin)) {
          g_options.stdin_list = 0;
          fprintf(stderr, "done with stdin\n");
          continue;
        }

        size_t len = strlen(buf);
        if(buf[len-1] == '\n') {
          buf[--len] = 0;
        }
        if(len > 0) {
          if(imv_navigator_add(&nav, buf, g_options.recursive)) {
            break;
          }
          need_redraw = 1;
        }
      }
    } else {
      SDL_Delay(10);
    }
  }
  while(g_options.list) {
    const char *path = imv_navigator_selection(&nav);
    if(!path) {
      break;
    }
    fprintf(stdout, "%s\n", path);
    imv_navigator_remove(&nav, path);
  }
  /* clean up our resources now that we're exiting */
  imv_destroy_loader(&ldr);
  imv_destroy_texture(&tex);
  imv_navigator_destroy(&nav);
  imv_destroy_viewport(&view);

  if(font) {
    TTF_CloseFont(font);
  }
  TTF_Quit();
  if(chequered_tex) {
    SDL_DestroyTexture(chequered_tex);
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}


/* vim:set ts=2 sts=2 sw=2 et: */
