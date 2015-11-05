#include <stdio.h>
#include <SDL2/SDL.h>

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
          if(e.key.keysym.sym == SDLK_q) {
            quit = 1;
          }
          break;
        case SDL_WINDOWEVENT:
          if(e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED
              || e.window.event == SDL_WINDOWEVENT_RESIZED) {
            redraw = 1;
          }
          break;
      }
    }

    if(quit) {
      break;
    }

    if(redraw) {
      SDL_RenderClear(renderer);
      //SDL_RenderCopy(renderer, tex, srcrect, dstrect);

      SDL_RenderPresent(renderer);
      redraw = 0;
    }
    SDL_Delay(10);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
