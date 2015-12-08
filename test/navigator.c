#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "navigator.h"

static void test_navigator_add(void **state)
{
  (void)state;
  struct imv_navigator nav;
  imv_navigator_init(&nav);

  assert_false(imv_navigator_poll_changed(&nav));
  imv_navigator_add(&nav, "path/to/some/file", 0);
  assert_true(imv_navigator_poll_changed(&nav));
  assert_string_equal(imv_navigator_selection(&nav), "path/to/some/file");

  imv_navigator_destroy(&nav);
}

int main(void)
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_navigator_add),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
