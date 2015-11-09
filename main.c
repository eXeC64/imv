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

SDL_Window *g_window = NULL;
SDL_Renderer *g_renderer = NULL;

struct loop_item_s {
  struct loop_item_s *prev;
  struct loop_item_s *next;
  const char *path;
};

struct {
  int autoscale;
  int fullscreen;
  int stdin;
} g_options = {0,0,0};

struct {
  double scale;
  int x, y;
  int fullscreen;
  int redraw;
} g_view = {1,0,0,0,1};

struct {
  struct loop_item_s *first, *last, *cur;
  int changed;
  int dir;
} g_path = {NULL,NULL,NULL,1,1};

struct {
  FIMULTIBITMAP *mbmp;
  FIBITMAP *frame; //most current bitmap frame
  int width, height; //overall dimensions of bitmap
  int cur_frame, next_frame, num_frames, playing; //animation state
  double frame_time;
} g_img = {NULL,NULL,0,0,0,0,0,0,0};

struct {
  int num_chunks; //number of chunks in use
  SDL_Texture **chunks; //array of SDL_Texture*
  int chunk_width, chunk_height; //dimensions of each chunk
  int num_chunks_wide; //number of chunks per row of the image
  int num_chunks_tall; //number of chunks per column of the image
  int max_chunk_width, max_chunk_height; //max dimensions a chunk can have
} g_gpu = {0,NULL,0,0,0,0,0,0};

void toggle_fullscreen()
{
  if(g_view.fullscreen) {
    SDL_SetWindowFullscreen(g_window, 0);
    g_view.fullscreen = 0;
  } else {
    SDL_SetWindowFullscreen(g_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    g_view.fullscreen = 1;
  }
}

void toggle_playing()
{
  if(g_img.playing) {
    g_img.playing = 0;
  } else if(g_img.num_frames >= 2) {
    g_img.playing = 1;
  }
}

void reset_view()
{
  g_view.scale = 1;
  g_view.x = 0;
  g_view.y = 0;
  g_view.redraw = 1;
}

void move_view(int x, int y)
{
  g_view.x += x;
  g_view.y += y;
  g_view.redraw = 1;
}

void zoom_view(int amount)
{
  g_view.scale += amount * 0.1;
  if(g_view.scale > 100)
    g_view.scale = 10;
  else if (g_view.scale < 0.01)
    g_view.scale = 0.1;
  g_view.redraw = 1;
}

void center_view()
{
  int wx, wy;
  SDL_GetWindowSize(g_window, &wx, &wy);
  g_view.x = (wx - g_img.width * g_view.scale) / 2;
  g_view.y = (wy - g_img.height * g_view.scale) / 2;
  g_view.redraw = 1;
}

void scale_to_window()
{
  int ww, wh;
  SDL_GetWindowSize(g_window, &ww, &wh);
  double window_aspect = (double)ww/(double)wh;
  double image_aspect = (double)g_img.width/(double)g_img.height;

  if(window_aspect > image_aspect) {
    //Image will become too tall before it becomes too wide
    g_view.scale = (double)wh/(double)g_img.height;
  } else {
    //Image will become too wide before it becomes too tall
    g_view.scale = (double)ww/(double)g_img.width;
  }
  //Also center image
  center_view();
  g_view.redraw = 1;
}

void add_path(const char* path)
{
  struct loop_item_s *new_path =
    (struct loop_item_s*)malloc(sizeof(struct loop_item_s));
  new_path->path = path;
  if(!g_path.first && !g_path.last) {
    g_path.first = new_path;
    g_path.last = new_path;
    new_path->next = new_path;
    new_path->prev = new_path;
    g_path.cur = new_path;
  } else {
    g_path.last->next = new_path;
    new_path->prev = g_path.last;
    g_path.first->prev = new_path;
    new_path->next = g_path.first;
    g_path.last = new_path;
  }
}

void remove_current_path()
{
  if(g_path.cur->next == g_path.cur) {
    fprintf(stderr, "All input files closed. Exiting\n");
    exit(0);
  }

  struct loop_item_s* cur = g_path.cur;
  cur->next->prev = cur->prev;
  cur->prev->next = cur->next;
  if(g_path.dir > 0) {
    g_path.cur = cur->prev;
  } else {
    g_path.cur = cur->next;
  }
  g_path.changed = 1;
  free(cur);
}

void next_path()
{
  g_path.cur = g_path.cur->prev;
  g_path.changed = 1;
  g_path.dir = 1;
}

void prev_path()
{
  g_path.cur = g_path.cur->next;
  g_path.changed = 1;
  g_path.dir = -1;
}

void push_img_to_gpu(FIBITMAP *image)
{
  if(g_img.frame) {
    FreeImage_Unload(g_img.frame);
  }
  g_img.frame = FreeImage_ConvertTo32Bits(image);
  g_img.width = FreeImage_GetWidth(g_img.frame);
  g_img.height = FreeImage_GetHeight(g_img.frame);

  char* pixels = (char*)FreeImage_GetBits(g_img.frame);

  //figure out how many chunks are needed, and create them
  if(g_gpu.num_chunks > 0) {
    for(int i = 0; i < g_gpu.num_chunks; ++i) {
      SDL_DestroyTexture(g_gpu.chunks[i]);
    }
    free(g_gpu.chunks);
  }

  g_gpu.num_chunks_wide = 1 + g_img.width / g_gpu.max_chunk_width;
  g_gpu.num_chunks_tall = 1 + g_img.height / g_gpu.max_chunk_height;

  int end_chunk_width = g_img.width % g_gpu.max_chunk_width;
  int end_chunk_height = g_img.height % g_gpu.max_chunk_height;

  if(end_chunk_width == 0) {
    end_chunk_width = g_gpu.max_chunk_width;
  }
  if(end_chunk_height == 0) {
    end_chunk_height = g_gpu.max_chunk_height;
  }

  g_gpu.num_chunks = g_gpu.num_chunks_wide * g_gpu.num_chunks_tall;
  g_gpu.chunks = 
    (SDL_Texture**)malloc(sizeof(SDL_Texture*) * g_gpu.num_chunks);

  int failed_at = -1;
  for(int y = 0; y < g_gpu.num_chunks_tall; ++y) {
    for(int x = 0; x < g_gpu.num_chunks_wide; ++x) {
      const int is_last_h_chunk = (x == g_gpu.num_chunks_wide - 1);
      const int is_last_v_chunk = (y == g_gpu.num_chunks_tall - 1);
      g_gpu.chunks[x + y * g_gpu.num_chunks_wide] =
        SDL_CreateTexture(g_renderer,
          SDL_PIXELFORMAT_RGB888,
          SDL_TEXTUREACCESS_STATIC,
          is_last_h_chunk ? end_chunk_width : g_gpu.max_chunk_width,
          is_last_v_chunk ? end_chunk_height : g_gpu.max_chunk_height);
      if(g_gpu.chunks[x + y * g_gpu.num_chunks_wide] == NULL) {

        failed_at = x + y * g_gpu.num_chunks_wide;
        break;
      }
    }
  }
  if(failed_at != -1) {
    for(int i = 0; i <= failed_at; ++i) {
      SDL_DestroyTexture(g_gpu.chunks[i]);
    }

    free(g_gpu.chunks);
    g_gpu.num_chunks = 0;
    fprintf(stderr, "SDL Error when creating texture: %s\n", SDL_GetError());
    return;
  }

  for(int y = 0; y < g_gpu.num_chunks_tall; ++y) {
    for(int x = 0; x < g_gpu.num_chunks_wide; ++x) {
      ptrdiff_t offset = 4 * x * g_gpu.max_chunk_width +
        y * 4 * g_img.width * g_gpu.max_chunk_height;
      char* addr = pixels + offset;
      SDL_UpdateTexture(g_gpu.chunks[x + y * g_gpu.num_chunks_wide],
          NULL, addr, 4 * g_img.width);
    }
  }

  g_view.redraw = 1;
}

void next_frame()
{
  if(g_img.num_frames < 2) {
    return;
  }
  FITAG *tag = NULL;
  char disposal_method = 0;
  int frame_time = 0;
  short top = 0;
  short left = 0;

  g_img.cur_frame = g_img.next_frame;
  g_img.next_frame = (g_img.cur_frame + 1) % g_img.num_frames;
  FIBITMAP *frame = FreeImage_LockPage(g_img.mbmp, g_img.cur_frame);
  FIBITMAP *frame32 = FreeImage_ConvertTo32Bits(frame);
  FreeImage_FlipVertical(frame32);

  //First frame is always going to use the raw frame
  if(g_img.cur_frame > 0) {
    FreeImage_GetMetadata(FIMD_ANIMATION, frame, "DisposalMethod", &tag);
    if(FreeImage_GetTagValue(tag)) {
      disposal_method = *(char*)FreeImage_GetTagValue(tag);
    }
  }

  FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameLeft", &tag);
  if(FreeImage_GetTagValue(tag)) {
    left = *(short*)FreeImage_GetTagValue(tag);
  }

  FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameTop", &tag);
  if(FreeImage_GetTagValue(tag)) {
    top = *(short*)FreeImage_GetTagValue(tag);
  }

  FreeImage_GetMetadata(FIMD_ANIMATION, frame, "FrameTime", &tag);
  if(FreeImage_GetTagValue(tag)) {
    frame_time = *(int*)FreeImage_GetTagValue(tag);
  }

  g_img.frame_time += frame_time * 0.001;

  FreeImage_UnlockPage(g_img.mbmp, frame, 0);

  //If this frame is inset, we need to expand it for compositing
  if(left != 0 || top != 0) {
    RGBQUAD color = {0,0,0,0};
    FIBITMAP *expanded = FreeImage_EnlargeCanvas(frame32,
        left,
        g_img.height - FreeImage_GetHeight(frame32) - top,
        g_img.width - FreeImage_GetWidth(frame32) - left,
        top,
        &color,
        0);
    FreeImage_Unload(frame32);
    frame32 = expanded;
  }

  switch(disposal_method) {
    case 0: //nothing specified, just use the raw frame
      push_img_to_gpu(frame32);
      break;
    case 1: //composite over previous frame
      if(g_img.frame && g_img.cur_frame > 0) {
        FIBITMAP *bg_frame = FreeImage_ConvertTo24Bits(g_img.frame);
        FIBITMAP *comp = FreeImage_Composite(frame32, 1, NULL, bg_frame);
        FreeImage_Unload(bg_frame);
        push_img_to_gpu(comp);
        FreeImage_Unload(comp);
      } else {
        //No previous frame, just render directly
        push_img_to_gpu(frame32);
      }
      break;
    case 2: //TODO - set to background, composite over that
      break;
    case 3: //TODO - restore to previous content
      break;
  }
  FreeImage_Unload(frame32);
}

int load_gif(const char* path)
{
  FIMULTIBITMAP *gif =
    FreeImage_OpenMultiBitmap(FIF_GIF, path,
        /* don't create */ 0,
        /* read only */ 1,
        /* keep in memory */ 1,
        /* flags */ GIF_LOAD256);

  if(!gif) {
    fprintf(stderr, "Error loading file: '%s'. Ignoring.\n", path);
    return 1;
  }

  g_img.mbmp = gif;
  g_img.num_frames = FreeImage_GetPageCount(gif);
  g_img.cur_frame = 0;
  g_img.next_frame = 0;
  g_img.frame_time = 0;
  g_img.playing = 1;

  next_frame();
  return 0;
}

int load_image(const char* path)
{
  if(g_img.mbmp) {
    FreeImage_CloseMultiBitmap(g_img.mbmp, 0);
    g_img.mbmp = NULL;
  }

  FREE_IMAGE_FORMAT fmt = FreeImage_GetFileType(path,0);

  if(fmt == FIF_UNKNOWN) {
    fprintf(stderr, "Could not identify file: '%s'. Ignoring.\n", path);
    return 1;
  }

  if(fmt == FIF_GIF) {
    return load_gif(path);
  }

  g_img.num_frames = 0;
  g_img.cur_frame = 0;
  g_img.next_frame = 0;
  g_img.frame_time = 0;
  g_img.playing = 0;

  FIBITMAP *image = FreeImage_Load(fmt, path, 0);
  if(!image) {
    fprintf(stderr, "Error loading file: '%s'. Ignoring.\n", path);
    return 1;
  }
  FIBITMAP *frame = FreeImage_ConvertTo32Bits(image);
  FreeImage_FlipVertical(frame);
  push_img_to_gpu(frame);
  FreeImage_Unload(image);
  return 0;
}

void print_usage(const char* name)
{
  fprintf(stdout,
  "Usage: %s [-ifsh] [images...]\n"
  "\n"
  "Flags:\n"
  "  -i: Read paths from stdin. One path per line.\n"
  "  -f: Start in fullscreen mode\n"
  "  -s: Auto scale images to fit window\n"
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
      case 'f': g_options.fullscreen = 1; break;
      case 's': g_options.autoscale = 1; break;
      case 'i': g_options.stdin = 1; break;
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

  for(int i = 1; i < argc; ++i) {
    if(argv[i][0] == '-') {
      parse_arg(argv[0], &argv[i][1]);
    } else {
      add_path(argv[i]);
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
        char *str = (char*)malloc(len + 1);
        memcpy(str, buf, len + 1);
        add_path(str);
      }
    }
  }

  if(SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL Failed to Init: %s\n", SDL_GetError());
    exit(1);
  }

  const int width = 1280;
  const int height = 720;

  g_window = SDL_CreateWindow(
        "imv",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_RESIZABLE);

  g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);

  //Use linear sampling for scaling
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

  //We need to know how big our textures can be
  SDL_RendererInfo ri;
  SDL_GetRendererInfo(g_renderer, &ri);
  g_gpu.max_chunk_width = ri.max_texture_width;
  g_gpu.max_chunk_height = ri.max_texture_height;

  //Put us in fullscren by default if requested
  if(g_options.fullscreen) {
    toggle_fullscreen();
  }

  double last_time = SDL_GetTicks() / 1000.0;

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
            case SDLK_q:     quit = 1;              break;
            case SDLK_LEFTBRACKET:
            case SDLK_LEFT:  prev_path();           break;
            case SDLK_RIGHTBRACKET:
            case SDLK_RIGHT: next_path();           break;
            case SDLK_EQUALS:
            case SDLK_i:
            case SDLK_UP:    zoom_view(1);          break;
            case SDLK_MINUS:
            case SDLK_o:
            case SDLK_DOWN:  zoom_view(-1);         break;
            case SDLK_r:     reset_view();          break;
            case SDLK_j:     move_view(0, -50);     break;
            case SDLK_k:     move_view(0, 50);      break;
            case SDLK_h:     move_view(50, 0);      break;
            case SDLK_l:     move_view(-50, 0);     break;
            case SDLK_x:     remove_current_path(); break;
            case SDLK_f:     toggle_fullscreen();   break;
            case SDLK_PERIOD:next_frame();          break;
            case SDLK_SPACE: toggle_playing();      break;
            case SDLK_s:     scale_to_window();     break;
          }
          break;
        case SDL_MOUSEWHEEL:
          zoom_view(e.wheel.y);
          break;
        case SDL_MOUSEMOTION:
          if(e.motion.state & SDL_BUTTON_LMASK) {
            move_view(e.motion.xrel, e.motion.yrel);
          }
          break;
        case SDL_WINDOWEVENT:
          g_view.redraw = 1;
          break;
      }
    }

    if(quit) {
      break;
    }

    while(g_path.changed) {
      if(load_image(g_path.cur->path) != 0) {
        remove_current_path();
      } else {
        g_path.changed = 0;
        char title[128];
        snprintf(&title[0], sizeof(title), "imv - %s", g_path.cur->path);
        SDL_SetWindowTitle(g_window, (const char*)&title);
        reset_view();
      }
      //Autoscale if requested
      if(g_options.autoscale) {
        scale_to_window();
      }
    }

    if(g_img.playing) {
      g_img.frame_time -= dt;

      while(g_img.frame_time < 0) {
        next_frame();
      }
    }

    if(g_view.redraw) {
      SDL_RenderClear(g_renderer);
      int offset_x = 0;
      int offset_y = 0;
      for(int y = 0; y < g_gpu.num_chunks_tall; ++y) {
        for(int x = 0; x < g_gpu.num_chunks_wide; ++x) {
          int img_w, img_h, img_access;
          unsigned int img_format;
          SDL_QueryTexture(g_gpu.chunks[x + y * g_gpu.num_chunks_wide],
              &img_format, &img_access, &img_w, &img_h);
          SDL_Rect g_view_area = {
            g_view.x + offset_x,
            g_view.y + offset_y,
            img_w * g_view.scale,
            img_h * g_view.scale
          };
          SDL_RenderCopy(g_renderer,
              g_gpu.chunks[x + y * g_gpu.num_chunks_wide], NULL, &g_view_area);
          offset_x += g_gpu.max_chunk_width * g_view.scale;
        }
        offset_x = 0;
        offset_y += g_gpu.max_chunk_height * g_view.scale;
      }

      SDL_RenderPresent(g_renderer);
      g_view.redraw = 0;
    }
    last_time = SDL_GetTicks() / 1000.0;
    SDL_Delay(10);
  }

  if(g_img.mbmp) {
    FreeImage_CloseMultiBitmap(g_img.mbmp, 0);
  }
  if(g_img.frame) {
    FreeImage_Unload(g_img.frame);
  }
  if(g_gpu.num_chunks > 0) {
    for(int i = 0; i < g_gpu.num_chunks; ++i) {
      SDL_DestroyTexture(g_gpu.chunks[i]);
    }
    free(g_gpu.chunks);
  }

  SDL_DestroyRenderer(g_renderer);
  SDL_DestroyWindow(g_window);
  SDL_Quit();

  return 0;
}
