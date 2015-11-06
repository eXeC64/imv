#include <SDL2/SDL.h>
#include <FreeImage.h>

SDL_Texture* imv_load_freeimage(SDL_Renderer *r, const char* path)
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
