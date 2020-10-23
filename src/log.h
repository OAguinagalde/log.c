/**
 * Copyright (c) 2020 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

// For use with android both these defines are needed:
// #define LOG_ANDROID
// #define LOG_ANDROID_ID "log_identifier"
// As well as the proper includes and libreries that the <android/log.h> requires

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#define LOG_VERSION "0.1.0"

typedef struct {
  va_list ap;
  const char *fmt;
  const char *file;
  struct tm *time;
  void *udata;
  int line;
  int level;
} log_Event;

typedef void (*log_LogFn)(log_Event *ev);
typedef void (*log_LockFn)(bool lock, void *udata);

enum { LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };

#ifndef NDEBUG
#   ifndef LOG_ANDROID
#       define log_trace(...) log_log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#       define log_debug(...) log_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#       define log_info(...)  log_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#       define log_warn(...)  log_log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#       define log_error(...) log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#       define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#   else
#       ifndef LOG_ANDROID_ID
#       error Both LOG_ANDROID and LOG_ANDROID_ID "identifier" need to be defined
#       endif // LOG_ANDROID_ID
#       include <android/log.h>
#       define log_trace(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_ANDROID_ID, __VA_ARGS__))
#       define log_debug(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_ANDROID_ID, __VA_ARGS__))
#       define log_info(...)  ((void)__android_log_print(ANDROID_LOG_INFO, LOG_ANDROID_ID, __VA_ARGS__))
#       define log_warn(...)  ((void)__android_log_print(ANDROID_LOG_WARN, LOG_ANDROID_ID, __VA_ARGS__))
#       define log_error(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_ANDROID_ID, __VA_ARGS__))
#       define log_fatal(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_ANDROID_ID, __VA_ARGS__))
#   endif // LOG_ANDROID
#else
#   define log_trace(...)
#   define log_debug(...)
#   define log_info(...)
#   define log_warn(...)
#   define log_error(...) log_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#   define log_fatal(...) log_log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#endif // NDEBUG


const char* log_level_string(int level);
void log_set_lock(log_LockFn fn, void *udata);
void log_set_level(int level);
void log_set_quiet(bool enable);
int log_add_callback(log_LogFn fn, void *udata, int level);
int log_add_fp(FILE *fp, int level);

void log_log(int level, const char *file, int line, const char *fmt, ...);

#endif
