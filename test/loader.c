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
  struct imv_loader *ldr = imv_loader_create();

  assert_false(SDL_Init(SDL_INIT_VIDEO));
  unsigned int NEW_IMAGE = SDL_RegisterEvents(1);
  unsigned int BAD_IMAGE = SDL_RegisterEvents(1);
  imv_loader_set_event_types(ldr, NEW_IMAGE, BAD_IMAGE);

  imv_loader_load(ldr, "test/orientation.jpg", NULL, 0);

  FIBITMAP *bmp = NULL;
  SDL_Event event;
  while(SDL_WaitEvent(&event)) {
    assert_false(event.type == BAD_IMAGE);

    if(event.type == NEW_IMAGE) {
      bmp = event.user.data1;
      break;
    }
  }

  assert_false(bmp == NULL);
  unsigned int width = FreeImage_GetWidth(bmp);
  assert_true(width == 1);

  FreeImage_Unload(bmp);
  imv_loader_free(ldr);
}

int main(void)
{
  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_jpeg_rotation),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}


/* vim:set ts=2 sts=2 sw=2 et: */
