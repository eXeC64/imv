#include "image.h"

struct imv_image *imv_image_create(SDL_Renderer *r)
{
  struct imv_image *image = malloc(sizeof(struct imv_image));
  memset(image, 0, sizeof(struct imv_image));
  image->renderer = r;

  SDL_RendererInfo ri;
  SDL_GetRendererInfo(r, &ri);
  image->chunk_width = ri.max_texture_width != 0 ? ri.max_texture_width : 4096;
  image->chunk_height = ri.max_texture_height != 0 ? ri.max_texture_height : 4096;
  return image;
}

void imv_image_free(struct imv_image *image)
{
  if(image->num_chunks > 0) {
    for(int i = 0; i < image->num_chunks; ++i) {
      SDL_DestroyTexture(image->chunks[i]);
    }
    free(image->chunks);
    image->num_chunks = 0;
    image->chunks = NULL;
    image->renderer = NULL;
  }
  free(image);
}

int imv_image_set_bitmap(struct imv_image *image, FIBITMAP *bmp)
{
  image->width = FreeImage_GetWidth(bmp);
  image->height = FreeImage_GetHeight(bmp);

  /* figure out how many chunks are needed, and create them */
  if(image->num_chunks > 0) {
    for(int i = 0; i < image->num_chunks; ++i) {
      SDL_DestroyTexture(image->chunks[i]);
    }
    free(image->chunks);
  }

  image->num_chunks_wide = 1 + image->width / image->chunk_width;
  image->num_chunks_tall = 1 + image->height / image->chunk_height;

  image->last_chunk_width = image->width % image->chunk_width;
  image->last_chunk_height = image->height % image->chunk_height;

  if(image->last_chunk_width == 0) {
    image->last_chunk_width = image->chunk_width;
  }
  if(image->last_chunk_height == 0) {
    image->last_chunk_height = image->chunk_height;
  }

  image->num_chunks = image->num_chunks_wide * image->num_chunks_tall;
  image->chunks = malloc(sizeof(SDL_Texture*) * image->num_chunks);

  int failed_at = -1;
  for(int y = 0; y < image->num_chunks_tall; ++y) {
    for(int x = 0; x < image->num_chunks_wide; ++x) {
      const int is_last_h_chunk = (x == image->num_chunks_wide - 1);
      const int is_last_v_chunk = (y == image->num_chunks_tall - 1);
      image->chunks[x + y * image->num_chunks_wide] =
        SDL_CreateTexture(image->renderer,
          SDL_PIXELFORMAT_ARGB8888,
          SDL_TEXTUREACCESS_STATIC,
          is_last_h_chunk ? image->last_chunk_width : image->chunk_width,
          is_last_v_chunk ? image->last_chunk_height : image->chunk_height);
      SDL_SetTextureBlendMode(image->chunks[x + y * image->num_chunks_wide],
          SDL_BLENDMODE_BLEND);
      if(image->chunks[x + y * image->num_chunks_wide] == NULL) {
        failed_at = x + y * image->num_chunks_wide;
        break;
      }
    }
  }

  if(failed_at != -1) {
    for(int i = 0; i <= failed_at; ++i) {
      SDL_DestroyTexture(image->chunks[i]);
    }
    free(image->chunks);
    image->num_chunks = 0;
    image->chunks = NULL;
    return 1;
  }

  BYTE* pixels = malloc(4 * image->width * image->height);
  FreeImage_ConvertToRawBits(pixels, bmp, 4 * image->width, 32,
      FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK, TRUE);

  for(int y = 0; y < image->num_chunks_tall; ++y) {
    for(int x = 0; x < image->num_chunks_wide; ++x) {
      ptrdiff_t offset = 4 * x * image->chunk_width +
        y * 4 * image->width * image->chunk_height;
      BYTE* addr = pixels + offset;
      SDL_UpdateTexture(image->chunks[x + y * image->num_chunks_wide],
          NULL, addr, 4 * image->width);
    }
  }

  free(pixels);
  return 0;
}

void imv_image_draw(struct imv_image *image, int bx, int by, double scale)
{
  int offset_x = 0;
  int offset_y = 0;
  for(int y = 0; y < image->num_chunks_tall; ++y) {
    for(int x = 0; x < image->num_chunks_wide; ++x) {
      int img_w, img_h;
      SDL_QueryTexture(image->chunks[x + y * image->num_chunks_wide], NULL, NULL,
        &img_w, &img_h);
      SDL_Rect view_area = {
        bx + offset_x,
        by + offset_y,
        img_w * scale,
        img_h * scale
      };
      SDL_RenderCopy(image->renderer,
          image->chunks[x + y * image->num_chunks_wide], NULL, &view_area);
      offset_x += image->chunk_width * scale;
    }
    offset_x = 0;
    offset_y += image->chunk_height * scale;
  }
}


/* vim:set ts=2 sts=2 sw=2 et: */
