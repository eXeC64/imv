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

#include "commands.h"
#include "list.h"
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

struct {
  struct imv_navigator nav;
  struct imv_loader *ldr;
  struct imv_texture *tex;
  struct imv_viewport *view;
  struct imv_commands *cmds;
  SDL_Window *window;
  SDL_Renderer *renderer;
  int quit;

  /* used to calculate when to skip to the next image in slideshow mode */
  unsigned long delay_msec;

  /* do we need to redraw the window? */
  int need_redraw;
  int need_rescale;

  /* buffer for command input - NULL when not in command mode */
  char *command_buffer;
} g_state;

void cmd_quit(struct imv_list *args);
void cmd_pan(struct imv_list *args);
void cmd_select_rel(struct imv_list *args);
void cmd_select_abs(struct imv_list *args);
void cmd_zoom(struct imv_list *args);
void cmd_remove(struct imv_list *args);
void cmd_fullscreen(struct imv_list *args);
void cmd_overlay(struct imv_list *args);

int main(int argc, char** argv)
{
  g_state.cmds = imv_commands_create();
  imv_command_register(g_state.cmds, "quit", &cmd_quit);
  imv_command_register(g_state.cmds, "pan", &cmd_pan);
  imv_command_register(g_state.cmds, "select_rel", &cmd_select_rel);
  imv_command_register(g_state.cmds, "select_abs", &cmd_select_abs);
  imv_command_register(g_state.cmds, "zoom", &cmd_zoom);
  imv_command_register(g_state.cmds, "remove", &cmd_remove);
  imv_command_register(g_state.cmds, "fullscreen", &cmd_fullscreen);
  imv_command_register(g_state.cmds, "overlay", &cmd_overlay);

  imv_command_alias(g_state.cmds, "q", "quit");
  imv_command_alias(g_state.cmds, "next", "select_rel 1");
  imv_command_alias(g_state.cmds, "previous", "select_rel -1");
  imv_command_alias(g_state.cmds, "n", "select_rel 1");
  imv_command_alias(g_state.cmds, "p", "select_rel -1");

  imv_navigator_init(&g_state.nav);

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
        if(imv_navigator_add(&g_state.nav, buf, g_options.recursive) != 0) {
          imv_navigator_destroy(&g_state.nav);
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
    if(imv_navigator_add(&g_state.nav, argv[i], g_options.recursive) != 0) {
      imv_navigator_destroy(&g_state.nav);
      exit(1);
    }
  }

  /* if we weren't given any paths we have nothing to view. exit */
  if(!imv_navigator_selection(&g_state.nav)) {
    fprintf(stderr, "No input files. Exiting.\n");
    exit(1);
  }

  /* go to the chosen starting image if possible */
  if(g_options.start_at) {
    int start_index = imv_navigator_find_path(&g_state.nav, g_options.start_at);
    if(start_index < 0) {
      start_index = strtol(g_options.start_at,NULL,10) - 1;
    }
    imv_navigator_select_str(&g_state.nav, start_index);
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

  g_state.window = SDL_CreateWindow(
        "imv",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_RESIZABLE);
  if(!g_state.window) {
    fprintf(stderr, "SDL Failed to create window: %s\n", SDL_GetError());
    SDL_Quit();
    exit(1);
  }

  /* we'll use SDL's built-in renderer, hardware accelerated if possible */
  g_state.renderer = SDL_CreateRenderer(g_state.window, -1, 0);
  if(!g_state.renderer) {
    fprintf(stderr, "SDL Failed to create renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(g_state.window);
    SDL_Quit();
    exit(1);
  }

  /* use the appropriate resampling method */
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
    g_options.nearest_neighbour ? "0" : "1");

  /* construct a chequered background texture */
  SDL_Texture *chequered_tex = NULL;
  if(!g_options.solid_bg) {
    chequered_tex = create_chequered(g_state.renderer);
  }

  /* set up the required fonts and surfaces for displaying the overlay */
  TTF_Init();
  TTF_Font *font = load_font(g_options.font);
  if(!font) {
    fprintf(stderr, "Error loading font: %s\n", TTF_GetError());
  }

  /* create our main classes */
  g_state.ldr = imv_loader_create();
  g_state.tex = imv_texture_create(g_state.renderer);
  g_state.view = imv_viewport_create(g_state.window);

  /* put us in fullscren mode to begin with if requested */
  if(g_options.fullscreen) {
    imv_viewport_toggle_fullscreen(g_state.view);
  }

  /* help keeping track of time */
  unsigned int last_time = SDL_GetTicks();
  unsigned int current_time;

  /* keep file change polling rate under control */
  static uint8_t poll_countdown = UINT8_MAX;

  g_state.need_redraw = 1;
  g_state.need_rescale = 0;

  /* keep title buffer around for reuse */
  char title[256];

  g_state.delay_msec = 0;

  /* start outside of command mode */
  g_state.command_buffer = NULL;

  /* initialize variables holding image dimentions */
  int iw = 0, ih = 0;

  g_state.quit = 0;
  while(!g_state.quit) {
    /* handle any input/window events sent by SDL */
    SDL_Event e;
    while(!g_state.quit && SDL_PollEvent(&e)) {
      switch(e.type) {
        case SDL_QUIT:
          imv_command_exec(g_state.cmds, "quit");
          break;

        case SDL_KEYDOWN:
          SDL_ShowCursor(SDL_DISABLE);

          if(g_state.command_buffer) {
            /* in command mode, update the buffer */
            if(e.key.keysym.sym == SDLK_ESCAPE) {
              free(g_state.command_buffer);
              g_state.command_buffer = NULL;
              g_state.need_redraw = 1;
            } else if(e.key.keysym.sym == SDLK_RETURN) {
              imv_command_exec(g_state.cmds, g_state.command_buffer);
              free(g_state.command_buffer);
              g_state.command_buffer = NULL;
              g_state.need_redraw = 1;
            } else if(e.key.keysym.sym == SDLK_BACKSPACE) {
              const size_t len = strlen(g_state.command_buffer);
              if(len > 0) {
                g_state.command_buffer[len - 1] = '\0';
                g_state.need_redraw = 1;
              }
            } else if(e.key.keysym.sym >= ' ' && e.key.keysym.sym <= '~') {
              const size_t len = strlen(g_state.command_buffer);
              if(len + 1 < 1024) {
                g_state.command_buffer[len] = e.key.keysym.sym;
                g_state.command_buffer[len+1] = '\0';
                g_state.need_redraw = 1;
              }
            }

            /* input has been consumed by command input, move onto next event */
            continue;
          }

          switch (e.key.keysym.sym) {
            case SDLK_SEMICOLON:
              if(e.key.keysym.mod & KMOD_SHIFT) {
                g_state.command_buffer = malloc(1024);
                g_state.command_buffer[0] = '\0';
                g_state.need_redraw = 1;
              }
              break;
            case SDLK_q:
              imv_command_exec(g_state.cmds, "quit");
              break;
            case SDLK_LEFTBRACKET:
            case SDLK_LEFT:
              imv_command_exec(g_state.cmds, "select_rel -1");
              break;
            case SDLK_RIGHTBRACKET:
            case SDLK_RIGHT:
              imv_command_exec(g_state.cmds, "select_rel 1");
              break;
            case SDLK_EQUALS:
            case SDLK_PLUS:
            case SDLK_i:
            case SDLK_UP:
              imv_viewport_zoom(g_state.view, g_state.tex, IMV_ZOOM_KEYBOARD, 1);
              break;
            case SDLK_MINUS:
            case SDLK_o:
            case SDLK_DOWN:
              imv_viewport_zoom(g_state.view, g_state.tex, IMV_ZOOM_KEYBOARD, -1);
              break;
            case SDLK_s:
              if(!e.key.repeat) {
                if((g_options.scaling += 1) > FULL) {
                  g_options.scaling = NONE;
                }
              }
            /* FALLTHROUGH */
            case SDLK_r:
              if(!e.key.repeat) {
                g_state.need_rescale = 1;
                g_state.need_redraw = 1;
              }
              break;
            case SDLK_a:
              if(!e.key.repeat) {
                imv_viewport_scale_to_actual(g_state.view, g_state.tex);
              }
              break;
            case SDLK_c:
              if(!e.key.repeat) {
                imv_viewport_center(g_state.view, g_state.tex);
              }
              break;
            case SDLK_j:
              imv_command_exec(g_state.cmds, "pan 0 -50");
              break;
            case SDLK_k:
              imv_command_exec(g_state.cmds, "pan 0 50");
              break;
            case SDLK_h:
              imv_command_exec(g_state.cmds, "pan 50 0");
              break;
            case SDLK_l:
              imv_command_exec(g_state.cmds, "pan -50 0");
              break;
            case SDLK_x:
              if(!e.key.repeat) {
                imv_command_exec(g_state.cmds, "remove");
              }
              break;
            case SDLK_f:
              if(!e.key.repeat) {
                imv_command_exec(g_state.cmds, "fullscreen");
              }
              break;
            case SDLK_PERIOD:
              imv_loader_load_next_frame(g_state.ldr);
              break;
            case SDLK_SPACE:
              if(!e.key.repeat) {
                imv_viewport_toggle_playing(g_state.view);
              }
              break;
            case SDLK_p:
              if(!e.key.repeat) {
                puts(imv_navigator_selection(&g_state.nav));
              }
              break;
            case SDLK_d:
              if(!e.key.repeat) {
                imv_command_exec(g_state.cmds, "overlay");
              }
              break;
            case SDLK_t:
              if(e.key.keysym.mod & (KMOD_SHIFT|KMOD_CAPS)) {
                if(g_options.delay >= 1000) {
                  g_options.delay -= 1000;
                }
              } else {
                g_options.delay += 1000;
              }
              g_state.need_redraw = 1;
              break;
          }
          break;
        case SDL_MOUSEWHEEL:
          imv_viewport_zoom(g_state.view, g_state.tex, IMV_ZOOM_MOUSE, e.wheel.y);
          SDL_ShowCursor(SDL_ENABLE);
          break;
        case SDL_MOUSEMOTION:
          if(e.motion.state & SDL_BUTTON_LMASK) {
            imv_viewport_move(g_state.view, e.motion.xrel, e.motion.yrel);
          }
          SDL_ShowCursor(SDL_ENABLE);
          break;
        case SDL_WINDOWEVENT:
          imv_viewport_update(g_state.view, g_state.tex);
          break;
      }
    }

    /* if we're quitting, don't bother drawing any more images */
    if(g_state.quit) {
      break;
    }

    /* check if an image failed to load, if so, remove it from our image list */
    char *err_path = imv_loader_get_error(g_state.ldr);
    if(err_path) {
      imv_navigator_remove(&g_state.nav, err_path);
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
    if(!g_options.cycle && imv_navigator_wrapped(&g_state.nav)) {
      break;
    }

    /* if the user has changed image, start loading the new one */
    if(imv_navigator_poll_changed(&g_state.nav, poll_countdown--)) {
      const char *current_path = imv_navigator_selection(&g_state.nav);
      if(!current_path) {
        if(g_options.stdin_list) {
          continue;
        }
        fprintf(stderr, "No input files left. Exiting.\n");
        exit(1);
      }

      snprintf(title, sizeof(title), "imv - [%i/%i] [LOADING] %s [%s]",
          g_state.nav.cur_path + 1, g_state.nav.num_paths, current_path,
          scaling_label[g_options.scaling]);
      imv_viewport_set_title(g_state.view, title);

      imv_loader_load(g_state.ldr, current_path, stdin_buffer, stdin_buffer_size);
      g_state.view->playing = 1;
    }

    /* get window height and width */
    int ww, wh;
    SDL_GetWindowSize(g_state.window, &ww, &wh);

    /* check if a new image is available to display */
    FIBITMAP *bmp;
    int is_new_image;
    if(imv_loader_get_image(g_state.ldr, &bmp, &is_new_image)) {
      imv_texture_set_image(g_state.tex, bmp);
      iw = FreeImage_GetWidth(bmp);
      ih = FreeImage_GetHeight(bmp);
      FreeImage_Unload(bmp);
      g_state.need_redraw = 1;
      g_state.need_rescale += is_new_image;
    }

    if(g_state.need_rescale) {
      g_state.need_rescale = 0;
      if(g_options.scaling == NONE ||
          (g_options.scaling == DOWN && ww > iw && wh > ih)) {
        imv_viewport_scale_to_actual(g_state.view, g_state.tex);
      } else {
        imv_viewport_scale_to_window(g_state.view, g_state.tex);
      }
    }

    current_time = SDL_GetTicks();

    /* if we're playing an animated gif, tell the loader how much time has
     * passed */
    if(g_state.view->playing) {
      unsigned int dt = current_time - last_time;
      /* We cap the delta-time to 100 ms so that if imv is asleep for several
       * seconds or more (e.g. suspended), upon waking up it doesn't try to
       * catch up all the time it missed by playing through the gif quickly. */
      if(dt > 100) {
        dt = 100;
      }
      imv_loader_time_passed(g_state.ldr, dt / 1000.0);
    }

    /* handle slideshow */
    if(g_options.delay) {
      unsigned int dt = current_time - last_time;

      g_state.delay_msec += dt;
      g_state.need_redraw = 1;
      if(g_state.delay_msec >= g_options.delay) {
        imv_navigator_select_rel(&g_state.nav, 1);
        g_state.delay_msec = 0;
      }
    }

    last_time = current_time;

    /* check if the viewport needs a redraw */
    if(imv_viewport_needs_redraw(g_state.view)) {
      g_state.need_redraw = 1;
    }

    /* only redraw when something's changed */
    if(g_state.need_redraw) {
      /* update window title */
      int len;
      const char *current_path = imv_navigator_selection(&g_state.nav);
      len = snprintf(title, sizeof(title), "imv - [%i/%i] [%ix%i] [%.2f%%] %s [%s]",
          g_state.nav.cur_path + 1, g_state.nav.num_paths, g_state.tex->width, g_state.tex->height,
          100.0 * g_state.view->scale,
          current_path, scaling_label[g_options.scaling]);
      if(g_options.delay >= 1000) {
        len += snprintf(title + len, sizeof(title) - len, "[%lu/%lus]",
            g_state.delay_msec / 1000 + 1, g_options.delay / 1000);
      }
      imv_viewport_set_title(g_state.view, title);

      /* first we draw the background */
      if(g_options.solid_bg) {
        /* solid background */
        SDL_SetRenderDrawColor(g_state.renderer,
            g_options.bg_r, g_options.bg_g, g_options.bg_b, 255);
        SDL_RenderClear(g_state.renderer);
      } else {
        /* chequered background */
        int img_w, img_h;
        SDL_QueryTexture(chequered_tex, NULL, NULL, &img_w, &img_h);
        /* tile the texture so it fills the window */
        for(int y = 0; y < wh; y += img_h) {
          for(int x = 0; x < ww; x += img_w) {
            SDL_Rect dst_rect = {x,y,img_w,img_h};
            SDL_RenderCopy(g_state.renderer, chequered_tex, NULL, &dst_rect);
          }
        }
      }

      /* draw our actual texture */
      imv_texture_draw(g_state.tex, g_state.view->x, g_state.view->y, g_state.view->scale);

      /* if the overlay needs to be drawn, draw that too */
      if(g_options.overlay && font) {
        SDL_Color fg = {255,255,255,255};
        SDL_Color bg = {0,0,0,160};
        imv_printf(g_state.renderer, font, 0, 0, &fg, &bg, "%s",
            title + strlen("imv - "));
      }

      /* draw command entry bar if needed */
      if(g_state.command_buffer && font) {
        SDL_Color fg = {255,255,255,255};
        SDL_Color bg = {0,0,0,160};
        imv_printf(g_state.renderer,
            font,
            0, wh - TTF_FontHeight(font),
            &fg, &bg,
            ":%s", g_state.command_buffer);
      }

      /* redraw complete, unset the flag */
      g_state.need_redraw = 0;

      /* reset poll countdown timer */
      poll_countdown = UINT8_MAX;

      /* tell SDL to show the newly drawn frame */
      SDL_RenderPresent(g_state.renderer);
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
          if(imv_navigator_add(&g_state.nav, buf, g_options.recursive)) {
            break;
          }
          g_state.need_redraw = 1;
        }
      }
    } else {
      SDL_Delay(10);
    }
  }
  while(g_options.list) {
    const char *path = imv_navigator_selection(&g_state.nav);
    if(!path) {
      break;
    }
    fprintf(stdout, "%s\n", path);
    imv_navigator_remove(&g_state.nav, path);
  }

  /* clean up our resources now that we're exiting */
  if(g_state.command_buffer) {
    free(g_state.command_buffer);
  }

  imv_loader_free(g_state.ldr);
  imv_texture_free(g_state.tex);
  imv_navigator_destroy(&g_state.nav);
  imv_viewport_free(g_state.view);
  imv_commands_free(g_state.cmds);

  if(font) {
    TTF_CloseFont(font);
  }
  TTF_Quit();
  if(chequered_tex) {
    SDL_DestroyTexture(chequered_tex);
  }
  SDL_DestroyRenderer(g_state.renderer);
  SDL_DestroyWindow(g_state.window);
  SDL_Quit();

  return 0;
}

void cmd_quit(struct imv_list *args)
{
  (void)args;
  g_state.quit = 1;
}

void cmd_pan(struct imv_list *args)
{
  if(args->len != 3) {
    return;
  }

  long int x = strtol(args->items[1], NULL, 10);
  long int y = strtol(args->items[2], NULL, 10);

  imv_viewport_move(g_state.view, x, y);
}

void cmd_select_rel(struct imv_list *args)
{
  if(args->len != 2) {
    return;
  }

  long int index = strtol(args->items[1], NULL, 10);
  imv_navigator_select_rel(&g_state.nav, index);

  /* reset slideshow delay */
  g_state.delay_msec = 0;
}

void cmd_select_abs(struct imv_list *args)
{
  (void)args;
}

void cmd_zoom(struct imv_list *args)
{
  (void)args;
}

void cmd_remove(struct imv_list *args)
{
  (void)args;
  char* path = strdup(imv_navigator_selection(&g_state.nav));
  imv_navigator_remove(&g_state.nav, path);
  free(path);

  /* reset slideshow delay */
  g_state.delay_msec = 0;
}

void cmd_fullscreen(struct imv_list *args)
{
  (void)args;
  imv_viewport_toggle_fullscreen(g_state.view);
}

void cmd_overlay(struct imv_list *args)
{
  (void)args;
  g_options.overlay = !g_options.overlay;
  g_state.need_redraw = 1;
}

/* vim:set ts=2 sts=2 sw=2 et: */
