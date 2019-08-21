#ifndef IMV_SOURCE_H
#define IMV_SOURCE_H

#include <pthread.h>
#include <stdbool.h>
#include "image.h"

struct imv_source_message {
  /* Pointer to sender of message */
  struct imv_source *source;

  /* User-supplied pointer */
  void *user_data;

  /* Receiver is responsible for destroying image */
  struct imv_image *image;

  /* If an animated gif, the frame's duration in milliseconds, else 0 */
  int frametime;

  /* Error message if image was NULL */
  const char *error;
};

/* While imv_image represents a single frame of an image, be it a bitmap or
 * vector image, imv_source represents an open handle to an image file, which
 * can emit one or more imv_images.
 */
struct imv_source {
  /* usually the path of the image this is the source of */
  char *name; 

  /* source's image dimensions */
  int width;
  int height;

  /* Usually 1, more if animated */
  int num_frames;

  /* Next frame to be loaded, 0-indexed */
  int next_frame;

  /* Attempted to be locked by load_first_frame or load_next_frame.
   * If the mutex can't be locked, the call is aborted.
   * Used to prevent the source from having multiple worker threads at once.
   * Released by the source before calling the message callback with a result.
   */
  pthread_mutex_t busy;

  /* Trigger loading of the first frame. Returns 0 on success. */
  void *(*load_first_frame)(struct imv_source *src);

  /* Trigger loading of next frame. Returns 0 on success. */
  void *(*load_next_frame)(struct imv_source *src);

  /* Safely free contents of this source. After this returns
   * it is safe to dealocate/overwrite the imv_source instance.
   */
  void (*free)(struct imv_source *src);

  /* User-specified callback for returning messages */
  void (*callback)(struct imv_source_message *message);

  /* User-specified pointer, included in returned messages */
  void *user_data;

  /* Implementation private data */
  void *private;
};


#endif
