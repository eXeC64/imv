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
#define FILENAME4 "example.file.4"
#define FILENAME5 "example.file.5"
#define FILENAME6 "example.file.6"

static void test_navigator_add_remove(void **state)
{
  (void)state;
  struct imv_navigator *nav = imv_navigator_create();

  /* Check poll_changed */
  assert_false(imv_navigator_poll_changed(nav));

  /* Add 6 paths, one non-existant should fail */
  assert_false(imv_navigator_add(nav, FILENAME1, 0));
  assert_false(imv_navigator_add(nav, FILENAME2, 0));
  assert_false(imv_navigator_add(nav, FILENAME3, 0));
  assert_false(imv_navigator_add(nav, FILENAME4, 0));
  assert_false(imv_navigator_add(nav, FILENAME5, 0));
  assert_false(imv_navigator_add(nav, FILENAME6, 0));
  assert_int_equal(imv_navigator_length(nav), 6);

  /* Check poll_changed */
  assert_true(imv_navigator_poll_changed(nav));

  /* Make sure current selection  is #1 */
  assert_string_equal(imv_navigator_selection(nav), FILENAME1);

  /* Move right and remove current file (#2); should get to #3 */
  imv_navigator_select_rel(nav, 1);
  assert_string_equal(imv_navigator_selection(nav), FILENAME2);
  imv_navigator_remove(nav, FILENAME2);
  assert_int_equal(imv_navigator_length(nav), 5);
  assert_string_equal(imv_navigator_selection(nav), FILENAME3);

  /* Move left and remove current file (#1); should get to #6 */
  imv_navigator_select_rel(nav, -1);
  assert_string_equal(imv_navigator_selection(nav), FILENAME1);
  imv_navigator_remove(nav, FILENAME1);
  assert_string_equal(imv_navigator_selection(nav), FILENAME6);

  /* Move left, right, remove current file (#6); should get to #3 */
  imv_navigator_select_rel(nav, -1);
  imv_navigator_select_rel(nav, 1);
  assert_string_equal(imv_navigator_selection(nav), FILENAME6);
  imv_navigator_remove(nav, FILENAME6);
  assert_string_equal(imv_navigator_selection(nav), FILENAME3);

  /* Remove #4; should not move */
  imv_navigator_remove(nav, FILENAME4);
  assert_string_equal(imv_navigator_selection(nav), FILENAME3);

  /* Verify that #4 is removed by moving left; should get to #5 */
  imv_navigator_select_rel(nav, 1);
  assert_string_equal(imv_navigator_selection(nav), FILENAME5);

  imv_navigator_free(nav);
}

static void test_navigator_file_changed(void **state)
{
  int fd;
  struct imv_navigator *nav = imv_navigator_create();
  struct timespec times[2] = { {0, 0}, {0, 0} };

  (void)state;

  fd = open(FILENAME1, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    imv_navigator_free(nav);
    (void)unlink(FILENAME1);
    skip();
  }
  assert_false(futimens(fd, times) == -1);

  assert_false(imv_navigator_add(nav, FILENAME1, 0));
  assert_true(imv_navigator_poll_changed(nav));
  assert_false(imv_navigator_poll_changed(nav));

  assert_false(sleep(1));

  fd = open(FILENAME1, O_RDWR);
  assert_false(fd == -1);

  times[0].tv_nsec = UTIME_NOW;
  times[0].tv_sec = UTIME_NOW;
  times[1].tv_nsec = UTIME_NOW;
  times[1].tv_sec = UTIME_NOW;
  assert_false(futimens(fd, times) == -1);

  /* sleep to ensure we don't hit the poll rate-limiting */
  sleep(1);

  assert_true(imv_navigator_poll_changed(nav));

  (void)close(fd);
  (void)unlink(FILENAME1);
  imv_navigator_free(nav);
}

int main(void)
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_navigator_add_remove),
    cmocka_unit_test(test_navigator_file_changed),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}


/* vim:set ts=2 sts=2 sw=2 et: */
