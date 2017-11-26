#ifndef IMV_BITMAP_H
#define IMV_BITMAP_H

struct imv_bitmap {
  int width;
  int height;
  unsigned char *data;
};

struct imv_bitmap *imv_bitmap_clone(struct imv_bitmap *bmp);
void imv_bitmap_free(struct imv_bitmap *bmp);

#endif
