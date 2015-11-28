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
#include <fontconfig/fontconfig.h>
#include <getopt.h>
#include <ctype.h>

#include "loader.h"
#include "texture.h"
#include "navigator.h"
#include "viewport.h"

SDL_Texture *create_chequered(SDL_Renderer *renderer);
int parse_hex_color(const char* str,
  unsigned char *r, unsigned char *g, unsigned char *b);

struct {
  int fullscreen;
  int stdin;
  int recursive;
  int actual;
  int nearest_neighbour;
  int solid_bg;
  unsigned char bg_r;
  unsigned char bg_g;
  unsigned char bg_b;
  int overlay;
  const char *start_at;
  const char *font;
} g_options = {0,0,0,0,0,1,0,0,0,0,NULL,"FreeMono:24"};

void print_usage(const char* name)
{
  fprintf(stdout,
  "imv %s\n"
  "Usage: %s [-rfaudh] [-n <NUM|PATH>] [-b BG] [-e FONT:SIZE] [-] [images...]\n"
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
  "  -e FONT:SIZE: Set the font used for the overlay. Defaults to FreeMono:24"
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

  while((o = getopt(argc, argv, "firaudhn:b:e:")) != -1) {
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
      case '?':
        fprintf(stderr, "Unknown argument '%c'. Aborting.\n", optopt);
        exit(1);
    }
  }
}

TTF_Font *load_font(const char *font_spec)
{
  int font_size;
  char *font_name;

  /* figure out font size from name, or default to 24 */
  char *sep = strchr(font_spec, ':');
  if(sep) {
    font_name = strndup(font_spec, sep - font_spec);
    font_size = strtol(sep+1, NULL, 10);
  } else {
    font_name = strdup(font_spec);
    font_size = 24;
  }


  FcConfig *cfg = FcInitLoadConfigAndFonts();
  FcPattern *pattern = FcNameParse((const FcChar8*)font_name);
  FcConfigSubstitute(cfg, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  TTF_Font *ret = NULL;

  FcResult result = FcResultNoMatch;
  FcPattern* font = FcFontMatch(cfg, pattern, &result);
  if (font) {
    FcChar8 *path = NULL;
    if (FcPatternGetString(font, FC_FILE, 0, &path) == FcResultMatch) {
      ret = TTF_OpenFont((char*)path, font_size);
    }
    FcPatternDestroy(font);
  }
  FcPatternDestroy(pattern);

  free(font_name);
  return ret;
}

int main(int argc, char** argv)
{
  if(argc < 2) {
    print_usage(argv[0]);
    exit(1);
  }

  struct imv_navigator nav;
  imv_init_navigator(&nav);

  parse_args(argc,argv);

  for(int i = optind; i < argc; ++i) {
    if(!strcmp("-",argv[i])) {
      g_options.stdin = 1;
      continue;
    }
    if(g_options.recursive) {
      imv_navigator_add_path_recursive(&nav, argv[i]);
    } else {
      imv_navigator_add_path(&nav, argv[i]);
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

  /* go to the chosen starting image if possible */
  if(g_options.start_at) {
    int start_index = imv_navigator_find_path(&nav, g_options.start_at);
    if(start_index < 0) {
      start_index = strtol(g_options.start_at,NULL,10) - 1;
    }
    imv_navigator_set_path(&nav, start_index);
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

  /* use the appropriate resampling method */
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
    g_options.nearest_neighbour ? "0" : "1");

  /* construct a chequered background texture */
  SDL_Texture *chequered_tex = create_chequered(renderer);

  TTF_Init();
  TTF_Font *font = load_font(g_options.font);
  if(!font) {
    fprintf(stderr, "Error loading font: %s\n", TTF_GetError());
  }
  SDL_Surface *overlay_surf = NULL;
  SDL_Texture *overlay_tex = NULL;

  struct imv_loader ldr;
  imv_init_loader(&ldr);

  struct imv_texture tex;
  imv_init_texture(&tex, renderer);

  struct imv_viewport view;
  imv_init_viewport(&view, window);

  /* Put us in fullscren by default if requested */
  if(g_options.fullscreen) {
    imv_viewport_toggle_fullscreen(&view);
  }

  double last_time = SDL_GetTicks() / 1000.0;

  int quit = 0;
  while(!quit) {
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
            case SDLK_UP:
              imv_viewport_zoom(&view, &tex, IMV_ZOOM_KEYBOARD, 1);
              break;
            case SDLK_MINUS:
            case SDLK_o:
            case SDLK_DOWN:
              imv_viewport_zoom(&view, &tex, IMV_ZOOM_KEYBOARD, -1);
              break;
            case SDLK_a:     imv_viewport_scale_to_actual(&view, &tex);break;
            case SDLK_r:     imv_viewport_scale_to_window(&view, &tex);break;
            case SDLK_c:      imv_viewport_center(&view, &tex);        break;
            case SDLK_j:      imv_viewport_move(&view, 0, -50);        break;
            case SDLK_k:      imv_viewport_move(&view, 0, 50);         break;
            case SDLK_h:      imv_viewport_move(&view, 50, 0);         break;
            case SDLK_l:      imv_viewport_move(&view, -50, 0);        break;
            case SDLK_x:      imv_navigator_remove_current_path(&nav); break;
            case SDLK_f:      imv_viewport_toggle_fullscreen(&view);   break;
            case SDLK_PERIOD: imv_loader_load_next_frame(&ldr);        break;
            case SDLK_SPACE:  imv_viewport_toggle_playing(&view, &tex);break;
            case SDLK_p:    puts(imv_navigator_get_current_path(&nav));break;
            case SDLK_d:
              g_options.overlay = !g_options.overlay;
              imv_viewport_set_redraw(&view);
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

    if(quit) {
      break;
    }

    char *err_path = imv_loader_get_error(&ldr);
    if(err_path) {
      imv_navigator_remove_path(&nav, err_path);
      free(err_path);
    }

    if(imv_navigator_has_changed(&nav)) {
      const char *current_path = imv_navigator_get_current_path(&nav);
      char title[256];
      snprintf(&title[0], sizeof(title), "imv - [%i/%i] [LOADING] %s",
          nav.cur_path + 1, nav.num_paths, current_path);
      imv_viewport_set_title(&view, title);

      if(!current_path) {
        fprintf(stderr, "No input files left. Exiting.\n");
        exit(1);
      }

      imv_loader_load_path(&ldr, current_path);
    }

    FIBITMAP *bmp = imv_loader_get_image(&ldr);
    if(bmp) {
      imv_texture_set_image(&tex, bmp);
      FreeImage_Unload(bmp);

      const char *current_path = imv_navigator_get_current_path(&nav);
      char title[256];
      snprintf(&title[0], sizeof(title), "imv - [%i/%i] [%ix%i] %s",
          nav.cur_path + 1, nav.num_paths,
          tex.width, tex.height, current_path);
      imv_viewport_set_title(&view, title);
      imv_viewport_scale_to_window(&view, &tex);
      if(g_options.actual) {
        imv_viewport_scale_to_actual(&view, &tex);
      }
    }

    if(view.playing) {
      double cur_time = SDL_GetTicks() / 1000.0;
      double dt = cur_time - last_time;
      last_time = SDL_GetTicks() / 1000.0;
      imv_loader_time_passed(&ldr, dt);
    }

    if(view.redraw) {
      if(g_options.solid_bg) {
        SDL_SetRenderDrawColor(renderer,
            g_options.bg_r, g_options.bg_g, g_options.bg_b, 255);
        SDL_RenderClear(renderer);
      } else {
        /* chequered background */
        int ww, wh;
        SDL_GetWindowSize(window, &ww, &wh);
        int img_w, img_h;
        SDL_QueryTexture(chequered_tex, NULL, NULL, &img_w, &img_h);
        for(int y = 0; y < wh; y += img_h) {
          for(int x = 0; x < ww; x += img_w) {
            SDL_Rect dst_rect = {x,y,img_w,img_h};
            SDL_RenderCopy(renderer, chequered_tex, NULL, &dst_rect);
          }
        }
      }
      imv_texture_draw(&tex, view.x, view.y, view.scale);
      if(g_options.overlay && font) {
        int ow, oh;
        SDL_QueryTexture(overlay_tex, NULL, NULL, &ow, &oh);
        SDL_Rect or = {0,0,ow,oh};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_RenderFillRect(renderer, &or);
        SDL_RenderCopy(renderer, overlay_tex, NULL, &or);
      }
      view.redraw = 0;
      SDL_RenderPresent(renderer);
    }
    SDL_Delay(10);
  }

  imv_destroy_loader(&ldr);
  imv_destroy_texture(&tex);
  imv_destroy_navigator(&nav);
  imv_destroy_viewport(&view);

  if(font) {
    TTF_CloseFont(font);
  }
  TTF_Quit();
  SDL_DestroyTexture(chequered_tex);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}

SDL_Texture *create_chequered(SDL_Renderer *renderer)
{
  SDL_RendererInfo ri;
  SDL_GetRendererInfo(renderer, &ri);
  int width = 512;
  int height = 512;
  if(ri.max_texture_width != 0 && ri.max_texture_width < width) {
    width = ri.max_texture_width;
  }
  if(ri.max_texture_height != 0 && ri.max_texture_height < height) {
    height = ri.max_texture_height;
  }
  const int box_size = 16;
  /* Create a chequered texture */
  const unsigned char l = 196;
  const unsigned char d = 96;

  size_t pixels_len = 3 * width * height;
  unsigned char *pixels = malloc(pixels_len);
  for(int y = 0; y < height; y++) {
    for(int x = 0; x < width; x += box_size) {
      unsigned char color = l;
      if(((x/box_size) % 2 == 0) ^ ((y/box_size) % 2 == 0)) {
        color = d;
      }
      memset(pixels + 3 * x + 3 * width * y, color, 3 * box_size);
    }
  }
  SDL_Texture *ret = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
    SDL_TEXTUREACCESS_STATIC,
    width, height);
  SDL_UpdateTexture(ret, NULL, pixels, 3 * width);
  free(pixels);
  return ret;
}

int parse_hex(char c) {
  if(c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

int parse_hex_color(const char* str,
  unsigned char *r, unsigned char *g, unsigned char *b)
{
  if(str[0] == '#') {
    ++str;
  }

  if(strlen(str) != 6) {
    return 1;
  }

  for(int i = 0; i < 6; ++i) {
    if(!isxdigit(str[i])) {
      return 1;
    }
  }

  *r = (parse_hex(str[0]) << 4) + parse_hex(str[1]);
  *g = (parse_hex(str[2]) << 4) + parse_hex(str[3]);
  *b = (parse_hex(str[4]) << 4) + parse_hex(str[5]);
  return 0;
}
