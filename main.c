/* Copyright (c) 2015 Harry Jeffery

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <stdio.h>
#include <stddef.h>
#include <SDL2/SDL.h>
#include <FreeImage.h>

#include "image.h"
#include "texture.h"
#include "navigator.h"
#include "viewport.h"

struct {
  int autoscale;
  int fullscreen;
  int stdin;
  int center;
  int recursive;
} g_options = {0,0,0,0,0};

void print_usage(const char* name)
{
  fprintf(stdout,
  "Usage: %s [-ifsh] [images...]\n"
  "\n"
  "Flags:\n"
  "  -i: Read paths from stdin. One path per line.\n"
  "  -r: Recursively search input paths.\n"
  "  -f: Start in fullscreen mode\n"
  "  -s: Auto scale images to fit window\n"
  "  -c: Center images in the window\n"
  "  -h: Print this help\n"
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
  "         's': Scale image to fit window\n"
  "         'x': Close current image\n"
  "         'f': Toggle fullscreen\n"
  "         ' ': Toggle gif playback\n"
  "         '.': Step a frame of gif playback\n"
  ,name);
}

void parse_arg(const char* name, const char* arg)
{
  for(const char *o = arg; *o != 0; ++o) {
    switch(*o) {
      case 'f': g_options.fullscreen = 1;   break;
      case 's': g_options.autoscale = 1;    break;
      case 'c': g_options.center = 1;       break;
      case 'i': g_options.stdin = 1;        break;
      case 'r': g_options.recursive = 1;    break;
      case 'h': print_usage(name); exit(0); break;
      default:
        fprintf(stderr, "Unknown argument '%c'. Aborting.\n", *o);
        exit(1);
    }
  }
}

int main(int argc, char** argv)
{
  if(argc < 2) {
    print_usage(argv[0]);
    exit(1);
  }

  struct imv_navigator nav;
  imv_init_navigator(&nav);

  for(int i = 1; i < argc; ++i) {
    if(argv[i][0] == '-') {
      parse_arg(argv[0], &argv[i][1]);
    } else {
      if(g_options.recursive) {
        imv_navigator_add_path_recursive(&nav, argv[i]);
      } else {
        imv_navigator_add_path(&nav, argv[i]);
      }
    }
  }

  if(g_options.stdin) {
    char buf[512];
    while(fgets(buf, sizeof(buf), stdin)) {
      size_t len = strlen(buf);
      if(buf[len-1] == '\n') {
        buf[--len] = 0;
      }
      if(len > 0) {
        if(g_options.recursive) {
          imv_navigator_add_path_recursive(&nav, buf);
        } else {
          imv_navigator_add_path(&nav, buf);
        }
      }
    }
  }

  if(!imv_navigator_get_current_path(&nav)) {
    fprintf(stderr, "No input files. Exiting.\n");
    exit(1);
  }

  if(SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL Failed to Init: %s\n", SDL_GetError());
    exit(1);
  }

  const int width = 1280;
  const int height = 720;

  SDL_Window *window = SDL_CreateWindow(
        "imv",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_RESIZABLE);

  SDL_Renderer *renderer =
    SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

  //Use linear sampling for scaling
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

  struct imv_image img;
  imv_init_image(&img);

  struct imv_texture tex;
  imv_init_texture(&tex, renderer);

  struct imv_viewport view;
  imv_init_viewport(&view, window);

  //Put us in fullscren by default if requested
  if(g_options.fullscreen) {
    imv_viewport_toggle_fullscreen(&view);
  }

  const char* current_path = NULL;

  double last_time = SDL_GetTicks() / 1000.0;
  int last_frame = img.cur_frame;

  int quit = 0;
  while(!quit) {
    double cur_time = SDL_GetTicks() / 1000.0;
    double dt = cur_time - last_time;

    SDL_Event e;
    while(!quit && SDL_PollEvent(&e)) {
      switch(e.type) {
        case SDL_QUIT:
          quit = 1;
          break;
        case SDL_KEYDOWN:
          switch (e.key.keysym.sym) {
            case SDLK_q:      quit = 1;                                break;
            case SDLK_LEFTBRACKET:
            case SDLK_LEFT:   imv_navigator_prev_path(&nav);           break;
            case SDLK_RIGHTBRACKET:
            case SDLK_RIGHT:  imv_navigator_next_path(&nav);           break;
            case SDLK_EQUALS:
            case SDLK_i:
            case SDLK_UP:     imv_viewport_zoom(&view, 1);             break;
            case SDLK_MINUS:
            case SDLK_o:
            case SDLK_DOWN:   imv_viewport_zoom(&view, -1);            break;
            case SDLK_r:      imv_viewport_reset(&view);               break;
            case SDLK_j:      imv_viewport_move(&view, 0, -50);        break;
            case SDLK_k:      imv_viewport_move(&view, 0, 50);         break;
            case SDLK_h:      imv_viewport_move(&view, 50, 0);         break;
            case SDLK_l:      imv_viewport_move(&view, -50, 0);        break;
            case SDLK_x:      imv_navigator_remove_current_path(&nav); break;
            case SDLK_f:      imv_viewport_toggle_fullscreen(&view);   break;
            case SDLK_PERIOD: imv_image_load_next_frame(&img);         break;
            case SDLK_SPACE:  imv_viewport_toggle_playing(&view, &img);break;
            case SDLK_s:     imv_viewport_scale_to_window(&view, &img);break;
          }
          break;
        case SDL_MOUSEWHEEL:
          imv_viewport_zoom(&view, e.wheel.y);
          break;
        case SDL_MOUSEMOTION:
          if(e.motion.state & SDL_BUTTON_LMASK) {
            imv_viewport_move(&view, e.motion.xrel, e.motion.yrel);
          }
          break;
        case SDL_WINDOWEVENT:
          imv_viewport_set_redraw(&view);
          break;
      }
    }

    if(quit) {
      break;
    }

    int resend = 0;
    while(imv_navigator_get_current_path(&nav) != current_path) {
      resend = 1;
      current_path = imv_navigator_get_current_path(&nav);

      if(!current_path) {
        fprintf(stderr, "No input files left. Exiting.\n");
        exit(1);
      }

      if(imv_image_load(&img, current_path) != 0) {
        imv_navigator_remove_current_path(&nav);
      } else {
        char title[128];
        snprintf(&title[0], sizeof(title), "imv - %s", current_path);
        SDL_SetWindowTitle(window, (const char*)&title);
        imv_viewport_reset(&view);
      }
      //Autoscale if requested
      if(g_options.autoscale) {
        imv_viewport_scale_to_window(&view, &img);
      }
      if(g_options.center) {
        imv_viewport_center(&view, &img);
      }
    }

    if(resend) {
      imv_texture_set_image(&tex, img.cur_bmp);
      imv_viewport_set_redraw(&view);
    }

    if(view.playing) {
      imv_image_play(&img, dt);
    }
    if(img.cur_frame != last_frame) {
      imv_texture_set_image(&tex, img.cur_bmp);
      imv_viewport_set_redraw(&view);
    }
    last_frame = img.cur_frame;

    if(view.redraw) {
      SDL_RenderClear(renderer);
      imv_texture_draw(&tex, view.x, view.y, view.scale);
      SDL_RenderPresent(renderer);
      view.redraw = 0;
    }
    last_time = SDL_GetTicks() / 1000.0;
    SDL_Delay(10);
  }

  imv_destroy_image(&img);
  imv_destroy_texture(&tex);
  imv_destroy_navigator(&nav);
  imv_destroy_viewport(&view);

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
