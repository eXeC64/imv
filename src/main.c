/* Copyright (c) 2015 Harry Jeffery

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

#include <stdio.h>
#include <stddef.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <FreeImage.h>
#include <getopt.h>
#include <ctype.h>

#include "loader.h"
#include "texture.h"
#include "navigator.h"
#include "viewport.h"
#include "util.h"

struct {
  int fullscreen;
  int stdin;
  int recursive;
  int actual;
  int nearest_neighbour;
  int solid_bg;
  int binary_stdin;
  unsigned int delay;
  unsigned char bg_r;
  unsigned char bg_g;
  unsigned char bg_b;
  int overlay;
  const char *start_at;
  const char *font;
} g_options = {0,0,0,0,0,1,0,0,0,0,0,0,NULL,"FreeMono:24"};

void print_usage(const char* name)
{
  fprintf(stdout,
  "imv %s\n"
  "Usage: %s [-rfaudh] [-n <NUM|PATH>] [-b BG] [-e FONT:SIZE] [-t SECONDS] [-] [images...]\n"
  "\n"
  "Flags:\n"
  "   -: Read paths from stdin. One path per line.\n"
  "  -r: Recursively search input paths.\n"
  "  -f: Start in fullscreen mode\n"
  "  -a: Default to images' actual size\n"
  "  -u: Use nearest neighbour resampling.\n"
  "  -d: Show overlay\n"
  "  -h: Print this help\n"
  "\n"
  "Options:\n"
  "  -n <NUM|PATH>: Start at picture number NUM, or with the path PATH.\n"
  "  -b BG: Set the background. Either 'checks' or a hex color value.\n"
  "  -e FONT:SIZE: Set the font used for the overlay. Defaults to FreeMono:24\n"
  "  -t SECONDS: Enable slideshow mode and set delay between images"
  "\n"
  "Mouse:\n"
  "   Click+Drag to Pan\n"
  "   MouseWheel to Zoom\n"
  "\n"
  "Hotkeys:\n"
  "         'q': Quit\n"
  "  '[',LArrow: Previous image\n"
  "  ']',RArrow: Next image\n"
  "     'i','+': Zoom in\n"
  "     'o','=': Zoom out\n"
  "         'h': Pan left\n"
  "         'j': Pan down\n"
  "         'k': Pan up\n"
  "         'l': Pan right\n"
  "         'r': Reset view\n"
  "         'a': Show image actual size\n"
  "         'c': Center view\n"
  "         'x': Close current image\n"
  "         'f': Toggle fullscreen\n"
  "         'd': Toggle overlay\n"
  "         ' ': Toggle gif playback\n"
  "         '.': Step a frame of gif playback\n"
  "         'p': Print current image path to stdout\n"
  "         't': Increase slideshow delay by one second\n"
  "         'T': Decrease slideshow delay by one second\n"
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
  , IMV_VERSION, name);
}

void parse_args(int argc, char** argv)
{
  /* Do not print getopt errors */
  opterr = 0;

  const char* name = argv[0];
  char o;

  while((o = getopt(argc, argv, "firaudhxn:b:e:t:")) != -1) {
    switch(o) {
      case 'f': g_options.fullscreen = 1;   break;
      case 'i':
        g_options.stdin = 1;
        fprintf(stderr, "Warning: '-i' is deprecated. Just use '-' instead.\n");
        break;
      case 'r': g_options.recursive = 1;           break;
      case 'a': g_options.actual = 1;              break;
      case 'u': g_options.nearest_neighbour = 1;   break;
      case 'd': g_options.overlay = 1;             break;
      case 'h': print_usage(name); exit(0);        break;
      case 'n':
        g_options.start_at = optarg;
        break;
      case 'b':
        if(strcmp("checks", optarg) == 0) {
          g_options.solid_bg = 0;
        } else {
          g_options.solid_bg = 1;
          if(parse_hex_color(optarg,
              &g_options.bg_r, &g_options.bg_g, &g_options.bg_b) != 0) {
            fprintf(stderr, "Invalid hex color: '%s'\n", optarg);
            exit(1);
          }
        }
        break;
      case 'e':
        g_options.font = optarg;
        break;
      case 't':
        g_options.delay = (unsigned int)atoi(optarg);
        break;
      case 'x':
        g_options.binary_stdin = 1;
        break;
      case '?':
        fprintf(stderr, "Unknown argument '%c'. Aborting.\n", optopt);
        exit(1);
    }
  }
}

int main(int argc, char** argv)
{
  char * stdin_buffer;
  size_t stdin_buffer_size;

  if(argc < 2) {
    print_usage(argv[0]);
    exit(1);
  }

  struct imv_navigator nav;
  imv_navigator_init(&nav);

  /* parse any command line options given */
  parse_args(argc, argv);
  /* if the user asked us to load both binary and paths from std in we should fail */
  if(g_options.stdin && g_options.binary_stdin){
    fprintf(stderr, "Binary stdin and read paths from stdin cannot" 
            "be used together. Exiting.\n");
    exit(1);
  }

  /* if user asked us to load binary image from stdin, now is the time */
  if(g_options.binary_stdin){
  imv_navigator_add(&nav, "stdin", 0);
    stdin_buffer = load_img_stdin(&stdin_buffer_size);
  }

  /* handle any image paths given as arguments */
  for(int i = optind; i < argc; ++i) {
    /* special case: '-' is actually an option */
    if(!strcmp("-",argv[i])) {
      g_options.stdin = 1;
      continue;
    }
    /* add the given path to the list to load */
    imv_navigator_add(&nav, argv[i], g_options.recursive);
  }

  /* if the user asked us to load paths from stdin, now is the time */
  if(g_options.stdin) {
    char buf[512];
    while(fgets(buf, sizeof(buf), stdin)) {
      size_t len = strlen(buf);
      if(buf[len-1] == '\n') {
        buf[--len] = 0;
      }
      if(len > 0) {
        imv_navigator_add(&nav, buf, g_options.recursive);
      }
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
  SDL_Texture *chequered_tex = create_chequered(renderer);

  /* set up the required fonts and surfaces for displaying the overlay */
  TTF_Init();
  TTF_Font *font = load_font(g_options.font);
  if(!font) {
    fprintf(stderr, "Error loading font: %s\n", TTF_GetError());
  }
  SDL_Surface *overlay_surf = NULL;
  SDL_Texture *overlay_tex = NULL;

  /* create our main classes on the stack*/
  struct imv_loader ldr;
  if(g_options.binary_stdin){
    imv_init_loader(&ldr, stdin_buffer, stdin_buffer_size);
  } else {
    imv_init_loader(&ldr, NULL, 0);
  }

  struct imv_texture tex;
  imv_init_texture(&tex, renderer);

  struct imv_viewport view;
  imv_init_viewport(&view, window);

  /* put us in fullscren mode to begin with if requested */
  if(g_options.fullscreen) {
    imv_viewport_toggle_fullscreen(&view);
  }

  /* help keeping track of time */
  unsigned int last_time;
  unsigned int current_time;

  /* used to keep track of when the selected image has changed */
  int is_new_image = 1;

  /* used to calculate when to skip to the next image in slideshow mode */
  unsigned int msecs_passed = 0;
  unsigned int delay_seconds_passed = 0;

  int quit = 0;
  while(!quit) {
    /* handle any input/window events sent by SDL */
    SDL_Event e;
    while(!quit && SDL_PollEvent(&e)) {
      switch(e.type) {
        case SDL_QUIT:
          quit = 1;
          break;
        case SDL_KEYDOWN:
          switch (e.key.keysym.sym) {
            case SDLK_q:
              quit = 1;
              break;
            case SDLK_LEFTBRACKET:
            case SDLK_LEFT:
              imv_navigator_select_rel(&nav, -1);
              /* reset slideshow delay */
              delay_seconds_passed = 0;
              break;
            case SDLK_RIGHTBRACKET:
            case SDLK_RIGHT:
              imv_navigator_select_rel(&nav, 1);
              /* reset slideshow delay */
              delay_seconds_passed = 0;
              break;
            case SDLK_EQUALS:
            case SDLK_i:
            case SDLK_UP:
              imv_viewport_zoom(&view, &tex, IMV_ZOOM_KEYBOARD, 1);
              break;
            case SDLK_MINUS:
            case SDLK_o:
            case SDLK_DOWN:
              imv_viewport_zoom(&view, &tex, IMV_ZOOM_KEYBOARD, -1);
              break;
            case SDLK_a:
              imv_viewport_scale_to_actual(&view, &tex);
              break;
            case SDLK_r:
              imv_viewport_scale_to_window(&view, &tex);
              break;
            case SDLK_c:
              imv_viewport_center(&view, &tex);
              break;
            case SDLK_j:
              imv_viewport_move(&view, 0, -50);
              break;
            case SDLK_k:
              imv_viewport_move(&view, 0, 50);
              break;
            case SDLK_h:
              imv_viewport_move(&view, 50, 0);
              break;
            case SDLK_l:
              imv_viewport_move(&view, -50, 0);
              break;
            case SDLK_x:
              imv_navigator_remove(&nav, imv_navigator_selection(&nav));
              break;
            case SDLK_f:
              imv_viewport_toggle_fullscreen(&view);
              break;
            case SDLK_PERIOD:
              imv_loader_load_next_frame(&ldr);
              break;
            case SDLK_SPACE:
              imv_viewport_toggle_playing(&view);
              break;
            case SDLK_p:
              puts(imv_navigator_selection(&nav));
              break;
            case SDLK_d:
              g_options.overlay = !g_options.overlay;
              imv_viewport_set_redraw(&view);
              break;
            case SDLK_t:
              if(e.key.keysym.mod & (KMOD_SHIFT|KMOD_CAPS)) {
                if(g_options.delay >= 1) {
                  g_options.delay--;
                }
              } else {
                g_options.delay++;
              }
              view.redraw = 1;
              break;
          }
          break;
        case SDL_MOUSEWHEEL:
          imv_viewport_zoom(&view, &tex, IMV_ZOOM_MOUSE, e.wheel.y);
          break;
        case SDL_MOUSEMOTION:
          if(e.motion.state & SDL_BUTTON_LMASK) {
            imv_viewport_move(&view, e.motion.xrel, e.motion.yrel);
          }
          break;
        case SDL_WINDOWEVENT:
          imv_viewport_updated(&view, &tex);
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
      free(err_path);
    }

    /* if the user has changed image, start loading the new one */
    if(imv_navigator_poll_changed(&nav)) {
      const char *current_path = imv_navigator_selection(&nav);
      if(!current_path) {
        fprintf(stderr, "No input files left. Exiting.\n");
        exit(1);
      }

      char title[256];
      snprintf(&title[0], sizeof(title), "imv - [%i/%i] [LOADING] %s",
          nav.cur_path + 1, nav.num_paths, current_path);
      imv_viewport_set_title(&view, title);

      imv_loader_load_path(&ldr, current_path);
      is_new_image = 1;
    }

    /* check if a new image is available to display */
    FIBITMAP *bmp = imv_loader_get_image(&ldr);
    if(bmp) {
      imv_texture_set_image(&tex, bmp);
      FreeImage_Unload(bmp);

      /* this is a new image, not just a new frame, so update our window's
       * title and the overlay */
      if(is_new_image) {
        is_new_image = 0;
        view.playing = 1;

        /* reset the viewport to fit the picture in the window */
        imv_viewport_scale_to_window(&view, &tex);

        /* or if they want images at their actual size, do that instead */
        if(g_options.actual) {
          imv_viewport_scale_to_actual(&view, &tex);
        }
      } else {
        /* just a new frame, tell the viewport it need to redraw */
        imv_viewport_updated(&view, &tex);
      }
    }

    current_time = SDL_GetTicks();

    /* if we're playing an animated gif, tell the loader how much time has
     * passed */
    if(view.playing) {
      unsigned int dt = current_time - last_time;
      imv_loader_time_passed(&ldr, dt / 1000.0);
    }

    /* handle slideshow */
    if (g_options.delay) {
      unsigned int dt = current_time - last_time;

      msecs_passed = msecs_passed + dt;
      /* tick every second */
      if(msecs_passed >= 1000) {
        msecs_passed = 0;
        delay_seconds_passed++;
        view.redraw = 1;
        if(delay_seconds_passed >= g_options.delay) {
          imv_navigator_select_rel(&nav, 1);
          delay_seconds_passed = 0;
        }
      }
    }

    last_time = current_time;

    /* only redraw when the view has changed, i.e. zoom/pan/window resized */
    if(view.redraw) {
      /* make sure to free any memory used by the old image */
      if(overlay_surf) {
        SDL_FreeSurface(overlay_surf);
        overlay_surf = NULL;
      }
      if(overlay_tex) {
        SDL_DestroyTexture(overlay_tex);
        overlay_tex = NULL;
      }

      /* update window title */
      const char *current_path = imv_navigator_selection(&nav);
      char title[256];
      if(g_options.delay) {
        snprintf(&title[0], sizeof(title), "imv - [%i/%i] [%i/%is] [%ix%i] %s",
            nav.cur_path + 1, nav.num_paths, delay_seconds_passed + 1,
            g_options.delay, tex.width, tex.height, current_path);
      } else {
        snprintf(&title[0], sizeof(title), "imv - [%i/%i] [%ix%i] %s",
            nav.cur_path + 1, nav.num_paths,
            tex.width, tex.height, current_path);
      }
      imv_viewport_set_title(&view, title);

      /* update the overlay */
      if(font) {
        if(g_options.delay) {
          snprintf(&title[0], sizeof(title), "[%i/%i] [%i/%is] %s",
              nav.cur_path + 1, nav.num_paths, delay_seconds_passed + 1,
              g_options.delay, current_path);
        } else {
          snprintf(&title[0], sizeof(title), "[%i/%i] %s", nav.cur_path + 1,
              nav.num_paths, current_path);
        }
        SDL_Color w = {255,255,255,255};
        overlay_surf = TTF_RenderUTF8_Blended(font, &title[0], w);
        overlay_tex = SDL_CreateTextureFromSurface(renderer, overlay_surf);
      }

      /* first we draw the background */
      if(g_options.solid_bg) {
        /* solid background */
        SDL_SetRenderDrawColor(renderer,
            g_options.bg_r, g_options.bg_g, g_options.bg_b, 255);
        SDL_RenderClear(renderer);
      } else {
        /* chequered background */
        int ww, wh;
        SDL_GetWindowSize(window, &ww, &wh);
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
        int ow, oh;
        SDL_QueryTexture(overlay_tex, NULL, NULL, &ow, &oh);
        SDL_Rect or = {0,0,ow,oh};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_RenderFillRect(renderer, &or);
        SDL_RenderCopy(renderer, overlay_tex, NULL, &or);
      }

      /* redraw complete, unset the flag */
      view.redraw = 0;

      /* tell SDL to show the newly drawn frame */
      SDL_RenderPresent(renderer);
    }

    /* sleep a little bit so we don't waste CPU time */
    SDL_Delay(10);
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
  if(overlay_surf) {
    SDL_FreeSurface(overlay_surf);
  }
  if(overlay_tex) {
    SDL_DestroyTexture(overlay_tex);
  }
  SDL_DestroyTexture(chequered_tex);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
