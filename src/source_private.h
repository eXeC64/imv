#ifndef IMV_SOURCE_PRIVATE_H
#define IMV_SOURCE_PRIVATE_H

struct imv_image;
struct imv_source;

/* This is the interface a source needs to implement to function correctly.
 * Backends act as a "factory" for sources by calling imv_source_create
 * with a pointer to a static vtable, and a pointer to that implementation's
 * private data, pre-initialised.
 */

struct imv_source_vtable {

  /* Loads the first frame, if successful puts output in image and duration
   * (in milliseconds) in frametime. If unsuccessful, image shall be NULL. A
   * still image should use a frametime of 0.
   */
  void (*load_first_frame)(void *private, struct imv_image **image, int *frametime);

  /* Loads the next frame, if successful puts output in image and duration
   * (in milliseconds) in frametime. If unsuccessful, image shall be NULL.
   */
  void (*load_next_frame)(void *private, struct imv_image **image, int *frametime);

  /* Cleans up the private data of a source */
  void (*free)(void *private);
};

/* Build a source given its vtable and a pointer to the private data */
struct imv_source *imv_source_create(const struct imv_source_vtable *vt, void *private);

#endif
