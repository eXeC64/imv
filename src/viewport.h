#ifndef IMV_VIEWPORT_H
#define IMV_VIEWPORT_H

#include <stdbool.h>
#include "image.h"

struct imv_viewport;

enum scaling_mode {
  SCALING_NONE,
  SCALING_DOWN,
  SCALING_FULL,
  SCALING_CROP,
  SCALING_MODE_COUNT
};

/* Used to signify how a a user requested a zoom */
enum imv_zoom_source {
  IMV_ZOOM_MOUSE,
  IMV_ZOOM_KEYBOARD,
  IMV_ZOOM_TOUCH
};

/* Creates an instance of imv_viewport */
struct imv_viewport *imv_viewport_create(int window_width, int window_height,
                                         int buffer_width, int buffer_height);

/* Cleans up an imv_viewport instance */
void imv_viewport_free(struct imv_viewport *view);

/* Set playback of animated gifs */
void imv_viewport_set_playing(struct imv_viewport *view, bool playing);

/* Get playback status of animated gifs */
bool imv_viewport_is_playing(struct imv_viewport *view);

/* Toggle playback of animated gifs */
void imv_viewport_toggle_playing(struct imv_viewport *view);

/* Fetch viewport offset/position */
void imv_viewport_get_offset(struct imv_viewport *view, int *x, int *y);

/* Fetch viewport scale */
void imv_viewport_get_scale(struct imv_viewport *view, double *scale);

/* Fetch viewport rotation */
void imv_viewport_get_rotation(struct imv_viewport *view, double *rotation);

/* Fetch viewport mirror status */
void imv_viewport_get_mirrored(struct imv_viewport *view, bool *mirrored);

/* Set the default pan_factor factor for the x and y position */
void imv_viewport_set_default_pan_factor(struct imv_viewport *view, double pan_factor_x, double pan_factor_y);

/* Pan the view by the given amounts without letting the image get too far
 * off-screen */
void imv_viewport_move(struct imv_viewport *view, int x, int y,
    const struct imv_image *image);

/* Pan the view relatively by coordinates */
void imv_viewport_move_relative(struct imv_viewport *view, int initial_x,
    int initial_y, int delta_x, int delta_y, struct imv_image *image);

/* Zoom the view by the given amount. imv_image* is used to get the image
 * dimensions */
void imv_viewport_zoom(struct imv_viewport *view, const struct imv_image *image,
                       enum imv_zoom_source, int mouse_x, int mouse_y, int amount);

/* Rotate the view by the given number of degrees */
void imv_viewport_rotate_by(struct imv_viewport *view, double degrees);

/* Rotate the view to the given number of degrees */
void imv_viewport_rotate_to(struct imv_viewport *view, double degrees);

/* Flip horizontally (across vertical axis) */
void imv_viewport_flip_h(struct imv_viewport *view);

/* Flip vertically (across horizontal axis) */
void imv_viewport_flip_v(struct imv_viewport *view);

/* Flip vertically (across horizontal axis) */
void imv_viewport_reset_transform(struct imv_viewport *view);

/* Recenter the view to be in the middle of the image */
void imv_viewport_center(struct imv_viewport *view,
                         const struct imv_image *image);

/* Scale the view so that the image appears at its actual resolution */
void imv_viewport_scale_to_actual(struct imv_viewport *view,
                                  const struct imv_image *image);

/*get scale when scaled to window*/
double imv_viewport_get_scale_to_window(struct imv_viewport *view,
					const struct imv_image *image);

/* Scale the view so that the image fits in the window */
void imv_viewport_scale_to_window(struct imv_viewport *view,
                                  const struct imv_image *image);

/* Scale the view so that the image fills the window */
void imv_viewport_crop_to_window(struct imv_viewport *view,
                                  const struct imv_image *image);

/* Rescale the view with the chosen scaling method */
void imv_viewport_rescale(struct imv_viewport *view, const struct imv_image *image,
                          enum scaling_mode);

/* Tell the viewport that it needs to be redrawn */
void imv_viewport_set_redraw(struct imv_viewport *view);

/* Tell the viewport the window or image has changed */
void imv_viewport_update(struct imv_viewport *view,
                         int window_width, int window_height,
                         int buffer_width, int buffer_height,
                         struct imv_image *image, enum scaling_mode);

/* Poll whether we need to redraw */
int imv_viewport_needs_redraw(struct imv_viewport *view);

#endif

/* vim:set ts=2 sts=2 sw=2 et: */
