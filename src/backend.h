#ifndef IMV_BACKEND_H
#define IMV_BACKEND_H

struct imv_source;

enum backend_result {
  BACKEND_SUCCESS = 0, /* successful load */
  BACKEND_BAD_PATH = 1, /* no such file, bad permissions, etc. */
  BACKEND_UNSUPPORTED = 2, /* unsupported file format */
};

/* A backend is responsible for taking a path, or a raw data pointer, and
 * converting that into a source that imv can handle. Each backend
 * may be powered by a different image library and support different
 * image formats.
 */
struct imv_backend {
  /* Name of the backend, for debug and user informational purposes */
  const char *name;

  /* Input: path to open
   * Output: initialises the imv_source instance passed in
   */
  enum backend_result (*open_path)(const char *path, struct imv_source **src);

  /* Clean up this backend */
  void (*free)(struct imv_backend *backend);
};

#endif
