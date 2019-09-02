#ifndef IMV_BACKEND_H
#define IMV_BACKEND_H

#include <stddef.h>

struct imv_source;

enum backend_result {

  /* The backend recognises the file's data and thinks it can load it */
  BACKEND_SUCCESS = 0,

  /* Represents a bad file or path, implies that other backends would also fail
   * and shouln't be tried.
   */
  BACKEND_BAD_PATH = 1,

  /* There's data, but this backend doesn't understand it. */
  BACKEND_UNSUPPORTED = 2,
};

/* A backend is responsible for taking a path, or a raw data pointer, and
 * converting that into an imv_source. Each backend may be powered by a
 * different image library and support different image formats.
 */
struct imv_backend {

  /* Name of the backend, for debug and user informational purposes */
  const char *name;

  /* Information about the backend, displayed by help dialog */
  const char *description;

  /* Official website address */
  const char *website;

  /* License the backend is used under */
  const char *license;

  /* Tries to open the given path. If successful, BACKEND_SUCCESS is returned
   * and src will point to an imv_source instance for the given path.
   */
  enum backend_result (*open_path)(const char *path, struct imv_source **src);

  /* Tries to read the passed data. If successful, BACKEND_SUCCESS is returned
   * and src will point to an imv_source instance for the given data.
   */
  enum backend_result (*open_memory)(void *data, size_t len, struct imv_source **src);
};

#endif
