#include <SDL2/SDL.h>
#include <string.h>
#include <jpeglib.h>
#include <setjmp.h>

SDL_Texture* imv_load_jpeg(SDL_Renderer *r, const char* path)
{
  FILE *fp = fopen(path, "rb");
  if(!fp) {
    return NULL;
  }

  struct jpeg_decompress_struct cinfo;
  jpeg_create_decompress(&cinfo);
  struct jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);

  jpeg_stdio_src(&cinfo, fp);
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);
  int row_stride = cinfo.output_width * cinfo.output_components;
  JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)
                ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
  char* pixels = (char*)malloc(row_stride*cinfo.output_height);
  while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, buffer, 1);
    void *out_row = pixels + row_stride * (cinfo.output_scanline-1);
    memcpy(out_row, buffer[0], row_stride);
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  fclose(fp);

  SDL_Texture *img = SDL_CreateTexture(r,
      SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC,
      cinfo.output_width, cinfo.output_height);

  SDL_Rect area = {0,0,cinfo.output_width,cinfo.output_height};
  SDL_UpdateTexture(img, &area, pixels, row_stride);
  free(pixels);
  return img;
}
