#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <unistd.h>
#include <FreeImage.h>
#include <pthread.h>
#include "loader.h"

static void test_jpeg_rotation(void **state)
{
  (void)state;
  struct imv_loader ldr;
  void *retval;
  char *error;
  unsigned int width;

  imv_init_loader(&ldr);

  imv_loader_load(&ldr, "test/orientation.jpg", NULL, 0);
  pthread_join(ldr.bg_thread, &retval);

  error = imv_loader_get_error(&ldr);
  assert_false(error);

  assert_false(retval == PTHREAD_CANCELED);
  assert_false(ldr.out_bmp == NULL);

  width = FreeImage_GetWidth(ldr.out_bmp);
  assert_true(width == 1);
}

int main(void)
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_jpeg_rotation),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
