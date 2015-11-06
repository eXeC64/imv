#include <stdio.h>
#include <SDL2/SDL.h>
#include <FreeImage.h>

struct loop_item_s {
  struct loop_item_s *prev;
  struct loop_item_s *next;
  const char *path;
};

struct {
  double scale;
  int x, y;
  int redraw;
} g_view = {1,0,0,1};

struct {
  struct loop_item_s *first, *last, *cur;
  int reload;
  int dir;
} g_path = {NULL,NULL,NULL,1,1};

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
  if(g_view.scale > 10)
    g_view.scale = 10;
  else if (g_view.scale < 1)
    g_view.scale = 1;
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
    g_path.last= new_path;
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
  g_path.reload = 1;
  free(cur);
}

void next_path()
{
  g_path.cur = g_path.cur->prev;
  g_path.reload = 1;
  g_path.dir = 1;
}

void prev_path()
{
  g_path.cur = g_path.cur->next;
  g_path.reload = 1;
  g_path.dir = -1;
}

SDL_Texture* load_freeimage(SDL_Renderer *r, const char* path)
{
  FREE_IMAGE_FORMAT fmt = FreeImage_GetFileType(path,0);
  FIBITMAP *image = FreeImage_Load(fmt, path, 0);
  FreeImage_FlipVertical(image);

  FIBITMAP* temp = image;
  image = FreeImage_ConvertTo32Bits(image);
  FreeImage_Unload(temp);

  int w = FreeImage_GetWidth(image);
  int h = FreeImage_GetHeight(image);
  char* pixels = (char*)FreeImage_GetBits(image);

  SDL_Texture *img = SDL_CreateTexture(r,
        SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STATIC, w, h);
  SDL_Rect area = {0,0,w,h};
  SDL_UpdateTexture(img, &area, pixels, sizeof(unsigned int) * w);
  FreeImage_Unload(image);
  return img;
}

int main(int argc, char** argv)
{
  if(argc < 2) {
    fprintf(stderr, "Usage: %s <image_file> <...>\n", argv[0]);
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

  for(int i = 1; i < argc; ++i) {
    add_path(argv[i]);
  }

  SDL_Texture *img = NULL;

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
            case SDLK_q:     quit = 1;              break;
            case SDLK_RIGHT: prev_path();           break;
            case SDLK_LEFT:  next_path();           break;
            case SDLK_i:
            case SDLK_UP:    zoom_view(1);          break;
            case SDLK_o:
            case SDLK_DOWN:  zoom_view(-1);         break;
            case SDLK_r:     reset_view();          break;
            case SDLK_j:     move_view(0, -50);     break;
            case SDLK_k:     move_view(0, 50);      break;
            case SDLK_h:     move_view(50, 0);      break;
            case SDLK_l:     move_view(-50, 0);     break;
            case SDLK_x:     remove_current_path(); break;
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

    while(g_path.reload) {
      if(img) {
        SDL_DestroyTexture(img);
        img = NULL;
      }
      img = load_freeimage(renderer, g_path.cur->path);
      if(img == NULL) {
        fprintf(stderr, "Ignoring unsupported file: %s\n", g_path.cur->path);
        remove_current_path();
      } else {
        g_path.reload = 0;
        reset_view();
      }
    }

    if(g_view.redraw && img) {
      SDL_RenderClear(renderer);

      if(img) {
        int img_w, img_h, img_access;
        unsigned int img_format;
        SDL_QueryTexture(img, &img_format, &img_access, &img_w, &img_h);
        SDL_Rect g_view_area = {
          g_view.x,
          g_view.y,
          img_w * g_view.scale,
          img_h * g_view.scale
        };
        SDL_RenderCopy(renderer, img, NULL, &g_view_area);
      }

      SDL_RenderPresent(renderer);
      g_view.redraw = 0;
    }
    SDL_Delay(10);
  }

  if(img) {
    SDL_DestroyTexture(img);
    img = NULL;
  }
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
