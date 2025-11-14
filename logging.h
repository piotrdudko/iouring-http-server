#pragma once

#include "userdata.h"

// Forward declarations to avoid circular dependency
struct regbuf_pool;
struct io_uring;

#include <liburing.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ASSERT_POSITIVE(code, ...)                                             \
  do {                                                                         \
    if ((code) < 0) {                                                          \
      fprintf(stderr,                                                          \
              "\033[37m[\033[0m\033[31mERROR\033[0m\033[37m]\033[0m %s%s%s. "  \
              "At %s:%d\n",                                                    \
              strerror(-code), sizeof((char[]){__VA_ARGS__}) == 1 ? "" : ": ",  \
              sizeof((char[]){__VA_ARGS__}) == 1 ? "" : (char[]){__VA_ARGS__}, \
              __FILE__, __LINE__);                                             \
      raise(SIGTRAP);                                                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#define ASSERT_NOT_NULL(ptr, ...)                                              \
  do {                                                                         \
    if ((ptr) == NULL) {                                                       \
      fprintf(stderr,                                                          \
              "\033[37m[\033[0m\033[31mERROR\033[0m\033[37m]\033[0m %s%s%s. "  \
              "At %s:%d\n",                                                    \
              strerror(errno), sizeof((char[]){__VA_ARGS__}) == 1 ? "" : ": ", \
              sizeof((char[]){__VA_ARGS__}) == 1 ? "" : (char[]){__VA_ARGS__}, \
              __FILE__, __LINE__);                                             \
      raise(SIGTRAP);                                                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#define ASSERT(condition, ...)                                                 \
  do {                                                                         \
    if ((condition)) {                                                         \
      fprintf(stderr,                                                          \
              "\033[37m[\033[0m\033[31mERROR\033[0m\033[37m]\033[0m %s%s%s. "  \
              "At %s:%d\n",                                                    \
              strerror(errno), sizeof((char[]){__VA_ARGS__}) == 1 ? "" : ": ", \
              sizeof((char[]){__VA_ARGS__}) == 1 ? "" : (char[]){__VA_ARGS__}, \
              __FILE__, __LINE__);                                             \
      raise(SIGTRAP);                                                          \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#define FMT_ADDRLEN 32
#define LOGGING_BUFSIZE 256
#define TRACE_LEVEL_BUFSIZE 8

#define COLOR_RESET   "\033[0m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RED     "\033[31m"
#define COLOR_BRIGHT_GREEN  "\033[92m"

extern char fmt_addr[FMT_ADDRLEN];

void format_inet_addr_from_sockfd(int sockfd, char *buf, size_t buf_sz);

enum log_level {
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR
};

void log_event(enum log_level level, struct io_uring *ring, struct regbuf_pool *bufpool, const char *fmt, ...);

#define debug_log(ring, bufpool, fmt, ...) \
    log_event(LOG_LEVEL_DEBUG, ring, bufpool, fmt, ##__VA_ARGS__)

#define info_log(ring, bufpool, fmt, ...) \
    log_event(LOG_LEVEL_INFO, ring, bufpool, fmt, ##__VA_ARGS__)

#define warn_log(ring, bufpool, fmt, ...) \
    log_event(LOG_LEVEL_WARN, ring, bufpool, fmt, ##__VA_ARGS__)

#define error_log(ring, bufpool, fmt, ...) \
    log_event(LOG_LEVEL_ERROR, ring, bufpool, fmt, ##__VA_ARGS__)
