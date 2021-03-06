/*
 * Copyright (c) 2020 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "log.h"

#define MAX_CALLBACKS 32

typedef struct {
  log_LogFn fn;
  void *udata;
  int level;
  // For cases where you want to store a path to a file, an application ID
  // or some other string. It's just easier to store it together with the callback.
  // Callbacks will use save_str in place of udata when udata == NULL.
  char save_str[256];
} Callback;

static struct {
  void *udata;
  log_LockFn lock;
  int level;
  bool quiet;
  Callback callbacks[MAX_CALLBACKS];
} L;


static const char *level_strings[] = {
  "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

#ifdef LOG_USE_COLOR
static const char *level_colors[] = {
  "\x1b[94;1m", "\x1b[36;1m", "\x1b[32;1m", "\x1b[33;1m", "\x1b[31;1m", "\x1b[35;1m"
};
#endif

// Unlike other Callbacks, this is a special case. It's de default and unless log_set_quiet(true),
// will allways run on log_log(). ev->udata is set to stderr.
static void stdout_callback(log_Event *ev) {
  char buf[16];
  buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)] = '\0';
#ifdef LOG_USE_COLOR
  fprintf(
    ev->udata, "%s %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
    buf, level_colors[ev->level], level_strings[ev->level],
    ev->file, ev->line);
#else
  fprintf(
    ev->udata, "%s %-5s %s:%d: ",
    buf, level_strings[ev->level], ev->file, ev->line);
#endif
  vfprintf(ev->udata, ev->fmt, ev->ap);
  fprintf(ev->udata, "\n");
  fflush(ev->udata);
}

#ifdef LOG_ANDROID
// ev->udata expected to be const char*, a log identifier for logcat.
static void android_log_callback(log_Event *ev) {
  // char buf[16];
  // buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)] = '\0';
  // #ifdef LOG_USE_COLOR
  //   __android_log_print(ev->level+2, ev->udata,
  //     "%s %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m",
  //     buf, level_colors[ev->level], level_strings[ev->level], ev->file, ev->line);
  // #else
  //   __android_log_print(ev->level+2, ev->udata,
  //     "%s %-5s %s:%d:",
  //     buf, level_strings[ev->level], ev->file, ev->line);
  // #endif
  __android_log_vprint(ev->level+2, ev->udata, ev->fmt, ev->ap);
}
#endif // LOG_ANDROID

// ev->udata expects a File*.
static void file_callback(log_Event *ev) {
  char buf[64];
  buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
  fprintf(
    ev->udata, "%s %-5s %s:%d: ",
    buf, level_strings[ev->level], ev->file, ev->line);
  vfprintf(ev->udata, ev->fmt, ev->ap);
  fprintf(ev->udata, "\n");
  fflush(ev->udata);
}

// ev->udata expects a const char*.
// The file gets opened, written to and closed on every call.
// Useful if you never know when the application might crash so that the file is closed.
static void file_bypath_callback(log_Event *ev) {
  FILE* f = fopen(ev->udata, "a+");
  char buf[64];
  buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)] = '\0';
  fprintf(
    f, "%s %-5s %s:%d: ",
    buf, level_strings[ev->level], ev->file, ev->line);
  vfprintf(f, ev->fmt, ev->ap);
  fprintf(f, "\n");
  fflush(f);
  fclose(f);
}


static void lock(void)   {
  if (L.lock) { L.lock(true, L.udata); }
}


static void unlock(void) {
  if (L.lock) { L.lock(false, L.udata); }
}


const char* log_level_string(int level) {
  return level_strings[level];
}


void log_set_lock(log_LockFn fn, void *udata) {
  L.lock = fn;
  L.udata = udata;
}


void log_set_level(int level) {
  L.level = level;
}


void log_set_quiet(bool enable) {
  L.quiet = enable;
}

// If !udata, then use save_str as udata
int log_add_callback(log_LogFn fn, void *udata, int level, char* save_str) {
  for (int i = 0; i < MAX_CALLBACKS; i++) {
    if (!L.callbacks[i].fn) {
      Callback cb;
      cb.fn = fn;
      cb.udata = udata;
      cb.level = level;

      if (save_str) {
        int index = 0;
        char c = save_str[index];
        while (c != '\0') {
          cb.save_str[index] = c;
          index++;
          c = save_str[index];
        }
        cb.save_str[index] = '\0';
      }

      L.callbacks[i] = cb;
      return 0;
    }
  }
  return -1;
}


int log_add_fp(FILE *fp, int level) {
  return log_add_callback(file_callback, fp, level, NULL);
}

bool log_add_filePath(char* path, int level, bool create_file) {
  bool OK = true;
  if (create_file) {
    FILE* f = fopen(path, "w+");
    OK = (f != NULL);
    assert(f && "Couldn't create file for log_add_filePath()! Make sure path is correct");
    fclose(f);
  }
  return OK && (log_add_callback(file_bypath_callback, NULL, level, path) == 0);
}

#ifdef LOG_ANDROID
int log_android_setup(char* logcat_identifier, int level) {
  // We don't need the default stderr printout for android, just add a callback which manages the android logging
  log_set_quiet(true);
  return log_add_callback(android_log_callback, NULL, level, logcat_identifier);
}
#endif // LOG_ANDROID

static void init_event(log_Event *ev, Callback *cb) {
  if (!ev->time) {
    time_t t = time(NULL);
    ev->time = localtime(&t);
  }
  if (cb->udata) {
    ev->udata = cb->udata;
  } else {
    ev->udata = &cb->save_str[0];
  }
}


void log_log(int level, const char *file, int line, const char *fmt, ...) {
  log_Event ev = {
    .fmt   = fmt,
    .file  = file,
    .line  = line,
    .level = level,
  };

  lock();

  if (!L.quiet && level >= L.level) {
    time_t t = time(NULL);
    ev.time = localtime(&t);
    ev.udata = stderr;
    va_start(ev.ap, fmt);
    stdout_callback(&ev);
    va_end(ev.ap);
  }

  for (int i = 0; i < MAX_CALLBACKS && L.callbacks[i].fn; i++) {
    Callback *cb = &L.callbacks[i];
    if (level >= cb->level) {
      init_event(&ev, cb);
      va_start(ev.ap, fmt);
      cb->fn(&ev);
      va_end(ev.ap);
    }
  }

  unlock();
}
