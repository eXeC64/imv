#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cmocka.h>
#include <errno.h>

#include "navigator.h"

#define FILENAME "example.file"

static void test_navigator_add(void **state)
{
  (void)state;
  struct imv_navigator nav;
  imv_navigator_init(&nav);

  assert_false(imv_navigator_poll_changed(&nav, 0));
  imv_navigator_add(&nav, FILENAME, 0);
  assert_true(imv_navigator_poll_changed(&nav, 0));
  assert_string_equal(imv_navigator_selection(&nav), FILENAME);

  imv_navigator_destroy(&nav);
}

static void test_navigator_file_changed(void **state)
{
  int fd;
  struct imv_navigator nav;
  struct timespec times[2] = { {0, 0}, {0, 0} };

  (void)state;
  imv_navigator_init(&nav);

  fd = open(FILENAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    imv_navigator_destroy(&nav);
    (void)unlink(FILENAME);
    skip();
  }
  assert_false(futimens(fd, times) == -1);

  imv_navigator_add(&nav, FILENAME, 0);
  assert_true(imv_navigator_poll_changed(&nav, 0));
  assert_false(imv_navigator_poll_changed(&nav, 0));

  assert_false(sleep(1));

  fd = open(FILENAME, O_RDWR);
  assert_false(fd == -1);

  times[0].tv_nsec = UTIME_NOW;
  times[0].tv_sec = UTIME_NOW;
  times[1].tv_nsec = UTIME_NOW;
  times[1].tv_sec = UTIME_NOW;
  assert_false(futimens(fd, times) == -1);

  assert_true(imv_navigator_poll_changed(&nav, 0));

  (void)close(fd);
  (void)unlink(FILENAME);
  imv_navigator_destroy(&nav);
}

int main(void)
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_navigator_add),
    cmocka_unit_test(test_navigator_file_changed),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
