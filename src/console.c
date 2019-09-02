#include "console.h"

#include "list.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/utext.h>
#include <unicode/ubrk.h>

struct imv_console {
  char *buffer;
  size_t buffer_len;
  size_t cursor;

  imv_console_callback callback;
  void *callback_data;

  struct list *history;
  ssize_t history_item; /* -1 when not traversing history */
  char *history_before; /* contents of line before history was opened */
};

/* Iterates forwards over characters in a UTF-8 string */
static size_t next_char(char *buffer, size_t position)
{
  size_t result = position;
  UErrorCode status = U_ZERO_ERROR;
  UText *ut = utext_openUTF8(NULL, buffer, -1, &status);

  UBreakIterator *it = ubrk_open(UBRK_CHARACTER, NULL, NULL, 0, &status);
  ubrk_setUText(it, ut, &status);

  int boundary = ubrk_following(it, (int)position);
  if (boundary != UBRK_DONE) {
    result = (size_t)boundary;
  }

  ubrk_close(it);

  utext_close(ut);
  assert(U_SUCCESS(status));
  return result;
}

/* Iterates backwards over characters in a UTF-8 string */
static size_t prev_char(char *buffer, size_t position)
{
  size_t result = position;
  UErrorCode status = U_ZERO_ERROR;
  UText *ut = utext_openUTF8(NULL, buffer, -1, &status);

  UBreakIterator *it = ubrk_open(UBRK_CHARACTER, NULL, NULL, 0, &status);
  ubrk_setUText(it, ut, &status);

  int boundary = ubrk_preceding(it, (int)position);
  if (boundary != UBRK_DONE) {
    result = (size_t)boundary;
  }

  ubrk_close(it);

  utext_close(ut);
  assert(U_SUCCESS(status));
  return result;
}

static void add_to_history(struct list *history, const char *line)
{
  /* Don't add blank lines */
  if (line[0] == '\0') {
    return;
  }

  /* Don't add the same line consecutively */
  if (history->len > 0 && !strcmp(history->items[history->len - 1], line)) {
    return;
  }

  list_append(history, strdup(line));
}

static void history_back(struct imv_console *console)
{
  if (console->history->len < 1) {
    /* No history to browse */
    return;
  }

  if (console->history_item == 0) {
    /* At the top of history. Do nothing. */
    return;
  }

  if (console->history_item == -1) {
    /* Enter history from the bottom */
    free(console->history_before);
    console->history_before = strdup(console->buffer);
    console->history_item = console->history->len - 1;
  } else {
    /* Move back in history */
    console->history_item--;
  }

  strncpy(console->buffer,
      console->history->items[console->history_item],
      console->buffer_len);
  console->cursor = strlen(console->buffer);
}

static void history_forward(struct imv_console *console)
{
  if (console->history_item == -1) {
    /* Not in history, do nothing */
    return;
  }

  if ((size_t)console->history_item == console->history->len - 1) {
    /* At the end of history, restore the backup */
    strncpy(console->buffer, console->history_before, console->buffer_len);
    console->cursor = strlen(console->buffer);
    free(console->history_before);
    console->history_before = NULL;
    console->history_item = -1;
  } else {
    /* Move forward in history */
    console->history_item++;
    strncpy(console->buffer,
        console->history->items[console->history_item],
        console->buffer_len);
    console->cursor = strlen(console->buffer);
  }

}

struct imv_console *imv_console_create(void)
{
  struct imv_console *console = calloc(1, sizeof *console);
  console->buffer_len = 1024;
  console->history_item = -1;
  console->history = list_create();
  return console;
}

void imv_console_free(struct imv_console *console)
{
  free(console->buffer);
  list_deep_free(console->history);
  free(console->history_before);
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
  console->cursor = 0;
  console->history_item = -1;
}

void imv_console_input(struct imv_console *console, const char *text)
{
  if (!console || !console->buffer) {
    return;
  }

  /* Don't allow newlines or control characters. */
  if (*text == '\n' || iscntrl(*text)) {
    return;
  }

  /* Increase buffer size if needed */
  if (strlen(text) + strlen(console->buffer) + 1 > console->buffer_len) {
    console->buffer_len *= 2;
    console->buffer = realloc(console->buffer, console->buffer_len);
    assert(console->buffer);
  }

  /* memmove over everything after the cursor right then copy in the new bytes */
  size_t to_insert = strlen(text);
  size_t old_cursor = console->cursor;
  size_t new_cursor = console->cursor + to_insert;
  size_t to_shift = strlen(console->buffer) - old_cursor;
  memmove(&console->buffer[new_cursor], &console->buffer[old_cursor], to_shift);
  memcpy(&console->buffer[old_cursor], text, to_insert);
  console->cursor = new_cursor;
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
    add_to_history(console->history, console->buffer);
    free(console->buffer);
    console->buffer = NULL;
    return true;
  }

  if (!strcmp("Left", key) || !strcmp("Ctrl+b", key)) {
    console->cursor = prev_char(console->buffer, console->cursor);
    return true;
  }

  if (!strcmp("Right", key) || !strcmp("Ctrl+f", key)) {
    console->cursor = next_char(console->buffer, console->cursor);
    return true;
  }

  if (!strcmp("Up", key) || !strcmp("Ctrl+p", key)) {
    history_back(console);
    return true;
  }

  if (!strcmp("Down", key) || !strcmp("Ctrl+n", key)) {
    history_forward(console);
    return true;
  }

  if (!strcmp("Ctrl+a", key)) {
    console->cursor = 0;
    return true;
  }

  if (!strcmp("Ctrl+e", key)) {
    console->cursor = strlen(console->buffer);
    return true;
  }

  if (!strcmp("BackSpace", key)) {
    /* memmove everything after the cursor left */
    size_t new_cursor = prev_char(console->buffer, console->cursor);
    size_t old_cursor = console->cursor;
    size_t to_shift = strlen(console->buffer) - new_cursor;
    memmove(&console->buffer[new_cursor], &console->buffer[old_cursor], to_shift);
    console->cursor = new_cursor;
    return true;
  }

  return false;
}

const char *imv_console_prompt(struct imv_console *console)
{
  return console->buffer;
}

size_t imv_console_prompt_cursor(struct imv_console *console)
{
  return console->cursor;
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
