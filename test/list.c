#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <unistd.h>
#include "list.h"

static void test_split_string(void **state)
{
  (void)state;

  struct imv_list *list;

  list = imv_split_string("word", ' ');
  assert_true(list);
  assert_true(list->len == 1);
  assert_false(strcmp(list->items[0], "word"));
  imv_list_deep_free(list);

  list = imv_split_string("hello world this is a test", ' ');
  assert_true(list);
  assert_true(list->len == 6);
  assert_false(strcmp(list->items[0], "hello"));
  assert_false(strcmp(list->items[1], "world"));
  assert_false(strcmp(list->items[2], "this"));
  assert_false(strcmp(list->items[3], "is"));
  assert_false(strcmp(list->items[4], "a"));
  assert_false(strcmp(list->items[5], "test"));
  imv_list_deep_free(list);

  list = imv_split_string("  odd  whitespace test  ", ' ');
  assert_true(list);
  assert_true(list->len == 3);
  assert_false(strcmp(list->items[0], "odd"));
  assert_false(strcmp(list->items[1], "whitespace"));
  assert_false(strcmp(list->items[2], "test"));
  imv_list_deep_free(list);
}

int main(void)
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_split_string),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}


/* vim:set ts=2 sts=2 sw=2 et: */
