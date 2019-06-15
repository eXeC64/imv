#include "keyboard.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>

struct imv_keyboard {
  struct xkb_context *context;
  struct xkb_keymap *keymap;
  struct xkb_state *state;
};

struct imv_keyboard *imv_keyboard_create(void)
{
  struct imv_keyboard *keyboard = calloc(1, sizeof *keyboard);
  keyboard->context = xkb_context_new(0);
  assert(keyboard->context);

  struct xkb_rule_names names = {
    .rules = NULL,
    .model = NULL,
    .layout = "gb",
    .variant = NULL,
    .options = "caps:escape,compose:ralt"
  };
  keyboard->keymap = xkb_keymap_new_from_names(keyboard->context, &names, 0);
  assert(keyboard->keymap);
  keyboard->state = xkb_state_new(keyboard->keymap);
  assert(keyboard->state);

  return keyboard;
}

void imv_keyboard_free(struct imv_keyboard *keyboard)
{
  if (!keyboard) {
    return;
  }
  xkb_state_unref(keyboard->state);
  keyboard->state = NULL;
  xkb_keymap_unref(keyboard->keymap);
  keyboard->keymap = NULL;
  xkb_context_unref(keyboard->context);
  keyboard->context = NULL;
  free(keyboard);
}

static const int scancode_offset = 8;

void imv_keyboard_update_key(struct imv_keyboard *keyboard, int scancode, bool pressed)
{
  xkb_state_update_key(keyboard->state, scancode + scancode_offset, pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
}

static const char *describe_prefix(struct imv_keyboard *keyboard)
{
  const bool ctrl = (xkb_state_mod_name_is_active(keyboard->state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0);
  const bool alt = (xkb_state_mod_name_is_active(keyboard->state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0);
  const bool shift = (xkb_state_mod_name_is_active(keyboard->state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0);

  if (ctrl && !alt && !shift) {
    return "Ctrl+";
  } else if (!ctrl && alt && !shift) {
    return "Meta+";
  } else if (!ctrl && !alt && shift) {
    return "Shift+";
  } else if (ctrl && alt && !shift) {
    return "Ctrl+Meta+";
  } else if (ctrl && !alt && shift) {
    return "Ctrl+Shift+";
  } else if (!ctrl && alt && shift) {
    return "Meta+Shift+";
  } else if (ctrl && alt && shift) {
    return "Ctrl+Meta+Shift+";
  } else {
    return "";
  }
}

size_t imv_keyboard_keyname(struct imv_keyboard *keyboard, int scancode, char *buf, size_t buflen)
{
  xkb_keysym_t keysym = xkb_state_key_get_one_sym(keyboard->state, scancode + scancode_offset);
  return xkb_keysym_get_name(keysym, buf, buflen);
}

char *imv_keyboard_describe_key(struct imv_keyboard *keyboard, int scancode)
{
  char keyname[128] = {0};
  imv_keyboard_keyname(keyboard, scancode, keyname, sizeof keyname);

  /* Modifier keys don't count on their own, only when pressed with another key */
  if (!strcmp(keyname, "Control_L") ||
      !strcmp(keyname, "Control_R") ||
      !strcmp(keyname, "Alt_L") ||
      !strcmp(keyname, "Alt_R") ||
      !strcmp(keyname, "Shift_L") ||
      !strcmp(keyname, "Shift_R") ||
      !strcmp(keyname, "Meta_L") ||
      !strcmp(keyname, "Meta_R") ||
      !strcmp(keyname, "Super_L") ||
      !strcmp(keyname, "Super_R")) {
    return NULL;
  }

  const char *prefix = describe_prefix(keyboard);

  char buf[128];
  snprintf(buf, sizeof buf, "%s%s", prefix, keyname);
  return strdup(buf);
}

size_t imv_keyboard_get_text(struct imv_keyboard *keyboard, int scancode, char *buf, size_t buflen)
{
  return xkb_state_key_get_utf8(keyboard->state, scancode + scancode_offset, buf, buflen);
}
