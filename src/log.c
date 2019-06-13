#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

static enum imv_log_level g_log_level = IMV_WARNING;

void imv_log_set_level(enum imv_log_level level)
{
  g_log_level = level;
}

void imv_log(enum imv_log_level level, const char *fmt,  ...)
{
  if (level > g_log_level) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}
