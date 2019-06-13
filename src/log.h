#ifndef IMV_LOG_H
#define IMV_LOG_H

enum imv_log_level {
  IMV_DEBUG,
  IMV_INFO,
  IMV_WARNING,
  IMV_ERROR
};

void imv_log_set_level(enum imv_log_level level);

void imv_log(enum imv_log_level level, const char *fmt,  ...);

#endif
