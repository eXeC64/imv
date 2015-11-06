#include <SDL2/SDL.h>
#include <tiffio.h>

SDL_Texture* imv_load_tiff(SDL_Renderer *r, const char* path)
{
  TIFF* tif = TIFFOpen(path, "r");
  if(!tif) {
    return NULL;
  }
  SDL_Texture *img = NULL;
  uint32 w, h;
  TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
  TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
  size_t npixels = w * h;
  uint32 *pixels = (uint32*) malloc(npixels * sizeof (uint32));
  if (TIFFReadRGBAImageOriented(tif, w, h, pixels, ORIENTATION_TOPLEFT, 0)) {
    img = SDL_CreateTexture(r,
        SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
    SDL_Rect area = {0,0,w,h};
    SDL_UpdateTexture(img, &area, pixels, sizeof(uint32) * w);
  }
  free(pixels);
  TIFFClose(tif);
  return img;
}

