#ifndef IMV_LOG_H
#define IMV_LOG_H

enum imv_log_level {
  IMV_DEBUG,
  IMV_INFO,
  IMV_WARNING,
  IMV_ERROR
};

/* Write to the log */
void imv_log(enum imv_log_level level, const char *fmt,  ...);

typedef void (*imv_log_callback)(enum imv_log_level level, const char *text, void *data);

/* Subscribe to the log, callback is called whenever a log entry is written */
void imv_log_add_log_callback(imv_log_callback callback, void *data);

/* Unsubscribe from the log */
void imv_log_remove_log_callback(imv_log_callback callback);

#endif
