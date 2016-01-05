#include <stdarg.h>
#include <stddef.h>
#include <fcntl.h>
#include <setjmp.h>
#include <cmocka.h>

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

int main(void)
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_navigator_add),
    cmocka_unit_test(test_navigator_file_changed),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
