#include <stdio.h>
#include <SDL2/SDL.h>

SDL_Texture* imv_load_image(SDL_Renderer *r, const char* path);

struct {
  double scale;
  int x, y;
  int redraw;
} g_view = {1,0,0,1};

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

  const char** paths = (const char**)&argv[1];
  const int num_paths = argc - 1;
  int cur_path = -1;
  int next_path = 0;
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
            case SDLK_q:
              quit = 1;
              break;
            case SDLK_RIGHT:
              next_path = (next_path + 1) % num_paths;
              break;
            case SDLK_LEFT:
              next_path -= 1;
              if(next_path < 0)
                next_path = num_paths - 1;
              break;
            case SDLK_i:
            case SDLK_UP:
              zoom_view(1);
              break;
            case SDLK_o:
            case SDLK_DOWN:
              zoom_view(-1);
              break;
            case SDLK_j:
              move_view(0, -50);
              break;
            case SDLK_k:
              move_view(0, 50);
              break;
            case SDLK_h:
              move_view(50, 0);
              break;
            case SDLK_l:
              move_view(-50, 0);
              break;
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

    if(next_path != cur_path) {
      cur_path = next_path;
      fprintf(stdout, "current image: %s\n", paths[cur_path]);
      if(img) {
        SDL_DestroyTexture(img);
        img = NULL;
      }
      img = imv_load_image(renderer, paths[cur_path]);
      reset_view();
    }

    if(g_view.redraw) {
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
