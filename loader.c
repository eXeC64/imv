#include <SDL2/SDL.h>
#include <string.h>

SDL_Texture* imv_load_png(SDL_Renderer *r, const char* path);
SDL_Texture* imv_load_jpeg(SDL_Renderer *r, const char* path);

SDL_Texture* imv_load_image(SDL_Renderer *r, const char* path)
{
  if(!path)
    return NULL;

  const char* ext = strrchr(path, '.');

  if(strcasecmp(ext, ".png") == 0) {
    return imv_load_png(r, path);
  } else if(strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".jpg") == 0) {
    return imv_load_jpeg(r, path);
  }

  fprintf(stderr,
      "Could not determine filetype of '%s' from its extension.\n",
      path);
  return NULL;
}
