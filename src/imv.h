#ifndef IMV_H
#define IMV_H

#include <stdbool.h>

struct imv;

struct imv *imv_create(void);
void imv_free(struct imv *imv);

bool imv_load_config(struct imv *imv);
bool imv_parse_args(struct imv *imv, int argc, char **argv);

void imv_add_path(struct imv *imv, const char *path);

int imv_run(struct imv *imv);

#endif

/* vim:set ts=2 sts=2 sw=2 et: */
