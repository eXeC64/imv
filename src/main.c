#include "imv.h"

struct imv_backend;

extern const struct imv_backend imv_backend_freeimage;
extern const struct imv_backend imv_backend_libpng;
extern const struct imv_backend imv_backend_librsvg;
extern const struct imv_backend imv_backend_libtiff;
extern const struct imv_backend imv_backend_libjpeg;
extern const struct imv_backend imv_backend_libnsgif;

int main(int argc, char **argv)
{
  struct imv *imv = imv_create();

  if (!imv) {
    return 1;
  }

#ifdef IMV_BACKEND_LIBTIFF
  imv_install_backend(imv, &imv_backend_libtiff);
#endif

#ifdef IMV_BACKEND_LIBPNG
  imv_install_backend(imv, &imv_backend_libpng);
#endif

#ifdef IMV_BACKEND_LIBJPEG
  imv_install_backend(imv, &imv_backend_libjpeg);
#endif

#ifdef IMV_BACKEND_LIBRSVG
  imv_install_backend(imv, &imv_backend_librsvg);
#endif

#ifdef IMV_BACKEND_LIBNSGIF
  imv_install_backend(imv, &imv_backend_libnsgif);
#endif

#ifdef IMV_BACKEND_FREEIMAGE
  imv_install_backend(imv, &imv_backend_freeimage);
#endif

  if (!imv_load_config(imv)) {
    imv_free(imv);
    return 1;
  }

  if (!imv_parse_args(imv, argc, argv)) {
    imv_free(imv);
    return 1;
  }

  int ret = imv_run(imv);

  imv_free(imv);

  return ret;
}

/* vim:set ts=2 sts=2 sw=2 et: */
