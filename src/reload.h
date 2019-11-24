#ifndef IMV_RELOAD_H
#define IMV_RELOAD_H

struct imv_reload *imv_reload_create(void);
void imv_reload_watch(struct imv_reload *rld, const char *path);
int imv_reload_changed(struct imv_reload *rld);
void imv_reload_free(struct imv_reload *rld);

#endif
