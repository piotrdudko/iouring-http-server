#pragma once

#include "bufring.h"
#include "userdata.h"

#include <arpa/inet.h>
#include <liburing.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
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

char fmt_addr[FMT_ADDRLEN];

void format_inet_addr_from_sockfd(int sockfd, char *buf, size_t buf_sz) {
  int err;
  int ip_len;
  struct sockaddr_in sender;
  socklen_t sender_sz = sizeof(struct sockaddr_in);

  err = getpeername(sockfd, (struct sockaddr *)&sender, &sender_sz);
  ASSERT_POSITIVE(-err);

  memset(buf, 0, buf_sz);
  inet_ntop(AF_INET, &sender.sin_addr, buf, buf_sz);

  ip_len = strlen(buf);
  snprintf(buf + ip_len, buf_sz - ip_len, ":%d", ntohs(sender.sin_port));
}

#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_BRIGHT_GREEN  "\033[92m"

void info(struct io_uring *ring, struct regbuf_pool *bufpool, const char *fmt, ...) {
  struct io_uring_sqe *sqe;
  struct timeval tv;
  struct tm *tm_info;
  char scratch1[LOGGING_BUFSIZE];
  char scratch2[LOGGING_BUFSIZE];
  size_t logsize;

  gettimeofday(&tv, NULL);
  tm_info = localtime(&tv.tv_sec);
  logsize = strftime(scratch1, LOGGING_BUFSIZE, "%Y-%m-%d %H:%M:%S", tm_info);
  snprintf(scratch1 + logsize, LOGGING_BUFSIZE - logsize, ".%03ld",
           tv.tv_usec / 1000);

  va_list args;
  va_start(args, fmt);
  logsize = vsnprintf(scratch2, LOGGING_BUFSIZE, fmt, args);
  va_end(args);

  struct regbuf_freelist_entry *regbuf = regbuf_pool_pop(bufpool);
  ASSERT_NOT_NULL(regbuf);
  char* log_buf = (char*)regbuf->iov->iov_base;
  uint16_t bid = regbuf->bid;

  logsize =
      snprintf(log_buf, LOGGING_BUFSIZE, "%s %s[INFO]%s %s\n", scratch1, COLOR_GREEN, COLOR_RESET, scratch2);

  sqe = io_uring_get_sqe(ring);
  ASSERT_NOT_NULL(sqe);
  io_uring_prep_write_fixed(sqe, STDERR_FILENO, log_buf, logsize, 0, bid);
  encode_userdata(sqe, bid, OP_WRITE_FIXED);
}
