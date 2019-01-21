#ifndef IMV_BACKEND_LIBPNG_H
#define IMV_BACKEND_LIBPNG_H

struct imv_backend;

/* Create an instance of the libpng backend */
const struct imv_backend *imv_backend_libpng(void);

#endif
