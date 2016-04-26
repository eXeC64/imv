#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cmocka.h>
#include <errno.h>

#include "navigator.h"

#define FILENAME1 "example.file.1"
#define FILENAME2 "example.file.2"
#define FILENAME3 "example.file.3"

static void test_navigator_add(void **state)
{
  (void)state;
  struct imv_navigator nav;
  imv_navigator_init(&nav);

  assert_false(imv_navigator_poll_changed(&nav, 0));
  imv_navigator_add(&nav, FILENAME1, 0);
  assert_true(imv_navigator_poll_changed(&nav, 0));
  assert_string_equal(imv_navigator_selection(&nav), FILENAME1);

  imv_navigator_destroy(&nav);
}

static void test_navigator_remove(void **state)
{
  (void)state;
  struct imv_navigator nav;
  imv_navigator_init(&nav);

  /* Add 3 paths */
  imv_navigator_add(&nav, FILENAME1, 0);
  imv_navigator_add(&nav, FILENAME2, 0);
  imv_navigator_add(&nav, FILENAME3, 0);
  assert_int_equal(nav.num_paths, 3);

  assert_string_equal(imv_navigator_selection(&nav), FILENAME1);
  assert_true(imv_navigator_select_rel(&nav, 1, 0));
  assert_string_equal(imv_navigator_selection(&nav), FILENAME2);
  imv_navigator_remove(&nav, FILENAME2);
  assert_int_equal(nav.num_paths, 2);
  assert_string_equal(imv_navigator_selection(&nav), FILENAME3);
  assert_false(imv_navigator_select_rel(&nav, 1, 0));

  assert_string_equal(imv_navigator_selection(&nav), FILENAME3);
  imv_navigator_remove(&nav, FILENAME3);
  assert_int_equal(nav.num_paths, 1);
  assert_string_equal(imv_navigator_selection(&nav), FILENAME1);
  imv_navigator_remove(&nav, FILENAME1);
  assert_int_equal(nav.num_paths, 0);
  assert_false(imv_navigator_selection(&nav));

  imv_navigator_destroy(&nav);
}

static void test_navigator_file_changed(void **state)
{
  int fd;
  struct imv_navigator nav;
  struct timespec times[2] = { {0, 0}, {0, 0} };

  (void)state;
  imv_navigator_init(&nav);

  fd = open(FILENAME1, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    imv_navigator_destroy(&nav);
    (void)unlink(FILENAME1);
    skip();
  }
  assert_false(futimens(fd, times) == -1);

  imv_navigator_add(&nav, FILENAME1, 0);
  assert_true(imv_navigator_poll_changed(&nav, 0));
  assert_false(imv_navigator_poll_changed(&nav, 0));

  assert_false(sleep(1));

  fd = open(FILENAME1, O_RDWR);
  assert_false(fd == -1);

  times[0].tv_nsec = UTIME_NOW;
  times[0].tv_sec = UTIME_NOW;
  times[1].tv_nsec = UTIME_NOW;
  times[1].tv_sec = UTIME_NOW;
  assert_false(futimens(fd, times) == -1);

  assert_true(imv_navigator_poll_changed(&nav, 0));

  (void)close(fd);
  (void)unlink(FILENAME1);
  imv_navigator_destroy(&nav);
}

int main(void)
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_navigator_add),
    cmocka_unit_test(test_navigator_remove),
    cmocka_unit_test(test_navigator_file_changed),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}


/* vim:set ts=2 sts=2 sw=2 et: */
