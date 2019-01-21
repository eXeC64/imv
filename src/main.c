#include "imv.h"

#include "backend.h"
#include "backend_freeimage.h"
#include "backend_libpng.h"
#include "backend_rsvg.h"

int main(int argc, char** argv)
{
  struct imv *imv = imv_create();

  if(!imv) {
    return 1;
  }

  imv_install_backend(imv, imv_backend_rsvg());
  imv_install_backend(imv, imv_backend_freeimage());
  imv_install_backend(imv, imv_backend_libpng());

  if(!imv_load_config(imv)) {
    imv_free(imv);
    return 1;
  }

  if(!imv_parse_args(imv, argc, argv)) {
    imv_free(imv);
    return 1;
  }

  int ret = imv_run(imv);

  imv_free(imv);

  return ret;
}

/* vim:set ts=2 sts=2 sw=2 et: */
