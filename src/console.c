#include "console.h"

#include <stdlib.h>
#include <string.h>

struct imv_console {
  char *buffer;
  size_t buffer_len;
  imv_console_callback callback;
  void *callback_data;
};

struct imv_console *imv_console_create(void)
{
  struct imv_console *console = calloc(1, sizeof *console);
  console->buffer_len = 1024;
  return console;
}

void imv_console_free(struct imv_console *console)
{
  if (console->buffer) {
    free(console->buffer);
    console->buffer = NULL;
  }
  free(console);
}

void imv_console_set_command_callback(struct imv_console *console,
    imv_console_callback callback, void *data)
{
  console->callback = callback;
  console->callback_data = data;
}

bool imv_console_is_active(struct imv_console *console)
{
  return console->buffer != NULL;
}

void imv_console_activate(struct imv_console *console)
{
  if (console->buffer) {
    return;
  }

  console->buffer = calloc(1, console->buffer_len);
}

void imv_console_input(struct imv_console *console, const char *text)
{
  if (!console || !console->buffer) {
    return;
  }

  strncat(console->buffer, text, console->buffer_len - 1);
}

bool imv_console_key(struct imv_console *console, const char *key)
{
  if (!console || !console->buffer) {
    return false;
  }

  if (!strcmp("Escape", key)) {
    free(console->buffer);
    console->buffer = NULL;
    return true;
  }

  if (!strcmp("Return", key)) {
    if (console->callback) {
      console->callback(console->buffer, console->callback_data);
    }
    free(console->buffer);
    console->buffer = NULL;
    return true;
  }

  if (!strcmp("BackSpace", key)) {
    const size_t len = strlen(console->buffer);
    if (len > 0) {
      console->buffer[len - 1] = '\0';
    }
    return true;
  }

  return false;
}

const char *imv_console_prompt(struct imv_console *console)
{
  return console->buffer;
}

const char *imv_console_backlog(struct imv_console *console)
{
  (void)console;
  return NULL;
}

void imv_console_write(struct imv_console *console, const char *text)
{
  (void)console;
  (void)text;
}

void imv_console_add_completion(struct imv_console *console, const char *template)
{
  (void)console;
  (void)template;
}
