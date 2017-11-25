#ifndef IMV_LOADER_H
#define IMV_LOADER_H

#include <unistd.h>

struct imv_loader;

/* Creates an instance of imv_loader */
struct imv_loader *imv_loader_create(void);

/* Cleans up an imv_loader instance */
void imv_loader_free(struct imv_loader *ldr);

/* Asynchronously load the given file */
void imv_loader_load(struct imv_loader *ldr, const char *path,
                     const void *buffer, const size_t buffer_size);

/* Set the custom event types for returning data */
void imv_loader_set_event_types(struct imv_loader *ldr,
    unsigned int new_image,
    unsigned int bad_image);

/* Trigger the next frame of the currently loaded image to be loaded and
 * returned as soon as possible. */
void imv_loader_load_next_frame(struct imv_loader *ldr);

/* Tell the loader that dt time has passed. If the current image is animated,
 * the loader will automatically load the next frame when it is due. */
void imv_loader_time_passed(struct imv_loader *ldr, double dt);

/* Ask the loader how long we can sleep for until the next frame */
double imv_loader_time_left(struct imv_loader *ldr);

#endif


/* vim:set ts=2 sts=2 sw=2 et: */
