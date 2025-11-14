#pragma once

#include "logging.h"

#include <liburing.h>
#include <stdint.h>

#define BUFRINGS_CONN 0
#define BUFRINGS_WRITE 0
#define BUFRINGS 2

#define REGISTERED_BUFFERS 16
#define REGISTERED_BUFFER_SIZE 1024

struct buffer_ring_init_params {
  uint16_t entries;
  uint16_t entry_size;
  uint16_t bgid;
};

struct buffer_ring {
  struct io_uring_buf_ring *br;
  struct iovec *bufs;
  struct buffer_ring_init_params params;
};

struct buffer_ring buffer_ring_init(struct io_uring *ring,
                                    struct buffer_ring_init_params params);

struct regbuf_pool {
  uint8_t *buf;
  struct iovec *iovecs;
  struct regbuf_freelist_entry *freelist_mem;
  struct regbuf_freelist_entry *freelist_head;
};

struct regbuf_freelist_entry {
  uint16_t bid;
  struct iovec *iov;
  struct regbuf_freelist_entry *next;
};

struct regbuf_pool regbuf_pool_init(struct io_uring *ring, size_t buf_size,
                                    size_t entries);
struct regbuf_freelist_entry *regbuf_pool_pop(struct regbuf_pool *self);
void regbuf_pool_put(struct regbuf_pool *self, int bid);
