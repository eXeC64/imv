#include <stdio.h>
#include <SDL2/SDL.h>

SDL_Texture* imv_load_image(SDL_Renderer *r, const char* path);

double clamp(double v, double min, double max) {
  if(v < min)
    return min;
  if(v > max)
    return max;
  return v;
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

  struct {
    double scale;
    int x, y;
  } view = {1,0,0};
  int quit = 0;
  int redraw = 1;
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
            case SDLK_UP:
              view.scale = clamp(view.scale + 0.1, 1, 10);
              redraw = 1;
              break;
            case SDLK_DOWN:
              view.scale = clamp(view.scale - 0.1, 1, 10);
              redraw = 1;
              break;
          }
          break;
        case SDL_MOUSEWHEEL:
          view.scale = clamp(view.scale + (0.1 * e.wheel.y), 1, 10);
          redraw = 1;
          break;
        case SDL_MOUSEMOTION:
          if(e.motion.state & SDL_BUTTON_LMASK) {
            view.x += e.motion.xrel;
            view.y += e.motion.yrel;
            redraw = 1;
          }
          break;
        case SDL_WINDOWEVENT:
          redraw = 1;
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
      view.scale = 1;
      view.x = 0;
      view.y = 0;
      redraw = 1;
    }

    if(redraw) {
      SDL_RenderClear(renderer);

      if(img) {
        int img_w, img_h, img_access;
        unsigned int img_format;
        SDL_QueryTexture(img, &img_format, &img_access, &img_w, &img_h);
        SDL_Rect view_area = {
          view.x,
          view.y,
          img_w * view.scale,
          img_h * view.scale
        };
        SDL_RenderCopy(renderer, img, NULL, &view_area);
      }

      SDL_RenderPresent(renderer);
      redraw = 0;
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
