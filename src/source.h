#ifndef IMV_SOURCE_H
#define IMV_SOURCE_H

#include <stdbool.h>
#include "bitmap.h"

struct imv_source_message {
  /* Pointer to sender of message */
  struct imv_source *source;

  /* User-supplied pointer */
  void *user_data;

  /* Receiver is responsible for destroying bitmap */
  struct imv_bitmap *bitmap;

  /* Error message if bitmap was NULL */
  const char *error;
};

/* Generic source of one or more bitmaps. Essentially a single image file */
struct imv_source {
  /* usually the path of the image this is the source of */
  char *name; 

  /* source's image dimensions */
  int width;
  int height;

  /* Usually 1, more if animated */
  int num_frames;

  /* Frames are returned using SDL events. These two fields must be
   * populated by callers before calling any frame loading functionality
   * on the source.
   */
  unsigned int image_event_id;
  unsigned int error_event_id;

  /* Trigger loading of the first frame. */
  void (*load_first_frame)(struct imv_source *src); 

  /* Trigger loading of next frame.  */
  void (*load_next_frame)(struct imv_source *src); 

  /* Notify source of time passing, automatically triggers loading of
   * the next frame when due. */
  void (*time_passed)(struct imv_source *src, double dt); 

  /* Asks the source how long we can sleep for before the next frame is due */
  double (*time_left)(struct imv_source *src);

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
