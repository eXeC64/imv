#include <SDL2/SDL.h>
#include <string.h>

extern SDL_Texture* imv_load_png(SDL_Renderer *r, const char* path);
extern SDL_Texture* imv_load_jpeg(SDL_Renderer *r, const char* path);

SDL_Texture* imv_load_image(SDL_Renderer *r, const char* path)
{
  if(!path)
    return NULL;

  const char* ext = strrchr(path, '.');
  if(ext == NULL) {
    fprintf(stderr,
        "Could not determine filetype of '%s' from its extension.\n",
        path);
  } else if(strcasecmp(ext, ".test") == 0) {
    const int width = 1280;
    const int height = 720;
    SDL_Texture *img = SDL_CreateTexture(r,
        SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STATIC,
        width, height);
    void *pixels = malloc(width*height*3);
    memset(pixels, 255, width*height*3);
    SDL_Rect area = {0,0,width,height};
    SDL_UpdateTexture(img, &area, pixels, width * 3);
    free(pixels);
    return img;
  } else if(strcasecmp(ext, ".png") == 0) {
    return imv_load_png(r, path);
  } else if(strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".jpg") == 0) {
    return imv_load_jpeg(r, path);
  }

  return NULL;
}
