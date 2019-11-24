#ifndef IMV_RELOAD_H
#define IMV_RELOAD_H

/* Create an imv_reload instance */
void imv_reload_free(struct imv_reload *rld);

/* Clean up an imv_reload instance */
struct imv_reload *imv_reload_create(void);

/* Start watching a path with inotify */
void imv_reload_watch(struct imv_reload *rld, const char *path);

/* Check if underlying file has been modified */
int imv_reload_changed(struct imv_reload *rld);

#endif
