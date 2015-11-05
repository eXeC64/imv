#include <SDL2/SDL.h>
#include <string.h>
#include <png.h>

SDL_Texture* imv_load_png(SDL_Renderer *r, const char* path)
{
  FILE *fp = fopen(path, "rb");

  if(!fp) {
    return NULL;
  }

  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png) {
    fclose(fp);
    return NULL;
  }

  png_infop info = png_create_info_struct(png);
  if(!info) {
    fclose(fp);
    png_destroy_read_struct(&png, NULL, NULL);
    return NULL;
  }

  png_init_io(png, fp);
  png_read_png(png, info,
      PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND,
      NULL);
  png_uint_32 width, height;
  png_get_IHDR(png, info, &width, &height, NULL, NULL, NULL, NULL, NULL);
  unsigned int bytes_per_row = png_get_rowbytes(png, info);
  png_bytepp row_ptrs = png_get_rows(png, info);

  char* pixels = (char*)malloc(bytes_per_row*height);
  for (unsigned int i = 0; i < height; i++) {
    memcpy(pixels + bytes_per_row * i, row_ptrs[i], bytes_per_row);
  }
  png_destroy_read_struct(&png, &info, NULL);
  fclose(fp);

  SDL_Texture *img = SDL_CreateTexture(r,
      SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC,
      width, height);

  SDL_Rect area = {0,0,width,height};
  SDL_UpdateTexture(img, &area, pixels, bytes_per_row);
  free(pixels);
  return img;
}
