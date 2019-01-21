#ifndef IMV_BACKEND_RSVG_H
#define IMV_BACKEND_RSVG_H

struct imv_backend;

/* Create an instance of the rsvg backend */
const struct imv_backend *imv_backend_librsvg(void);

#endif
