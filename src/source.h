#ifndef IMV_SOURCE_H
#define IMV_SOURCE_H

/* While imv_image represents a single frame of an image, be it a bitmap or
 * vector image, imv_source represents an open handle to an image file, which
 * can emit one or more imv_images.
 */
struct imv_source;

struct imv_source_message;
struct imv_image;

/* Clean up a source. Blocks if the source is active in the background. Async
 * version does not block, performing cleanup in another thread */
void imv_source_async_free(struct imv_source *src);
void imv_source_free(struct imv_source *src);

/* Load the first frame. Silently aborts if source is already loading. Async
 * version performs loading in background. */
void imv_source_async_load_first_frame(struct imv_source *src);
void imv_source_load_first_frame(struct imv_source *src);

/* Load the next frame. Silently aborts if source is already loading. Async
 * version performs loading in background. */
void imv_source_async_load_next_frame(struct imv_source *src);
void imv_source_load_next_frame(struct imv_source *src);

typedef void (*imv_source_callback)(struct imv_source_message *message);

/* Sets the callback function to be called when frame loading completes */
void imv_source_set_callback(struct imv_source *src, imv_source_callback callback, void *data);

struct imv_source_message {
  /* Pointer to sender of message */
  struct imv_source *source;

  /* User-supplied pointer */
  void *user_data;

  /* Receiver is responsible for destroying image */
  struct imv_image *image;

  /* If an animated gif, the frame's duration in milliseconds, else 0 */
  int frametime;
};

#endif
