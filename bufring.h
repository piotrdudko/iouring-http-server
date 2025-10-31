#pragma once

#include "logging.h"

#include <stdlib.h>
#include <stdint.h>
#include <liburing.h>

#define BUFRINGS_CONN 0
#define BUFRINGS_WRITE 0
#define BUFRINGS 2

#define REGISTERED_BUFFERS 16
#define REGISTERED_BUFFER_SIZE 1024

struct buffer_ring_init_params {
  size_t entries;
  size_t entry_size;
  int bgid;
};

struct buffer_ring {
  struct io_uring_buf_ring *br;
  struct iovec *bufs;
  struct buffer_ring_init_params params;
};

struct buffer_ring buffer_ring_init(struct io_uring *ring,
                                    struct buffer_ring_init_params params) {
  struct io_uring_buf_ring *br;
  size_t i;
  int res;

  uint8_t *_bufs = (uint8_t*) malloc(params.entries * params.entry_size);
  struct iovec *bufs = (struct iovec*) malloc(params.entries * sizeof(struct iovec));

  /* allocate mem for sharing buffer ring */
  res = posix_memalign((void **)&br, 4096,
                       params.entries * sizeof(struct io_uring_buf_ring));
  ASSERT_POSITIVE(-res);

  /* assign and register buffer ring */
  struct io_uring_buf_reg reg = {.ring_addr = (unsigned long)br,
                                 .ring_entries = params.entries,
                                 .bgid = params.bgid};
  res = io_uring_register_buf_ring(ring, &reg, 0);
  ASSERT_POSITIVE(res);

  /* add initial buffers to the ring */
  io_uring_buf_ring_init(br);
  for (i = 0; i < params.entries; i++) {
    bufs[i].iov_base = _bufs + (i * params.entry_size);
    bufs[i].iov_len = params.entry_size;

    /* add each buffer, we'll use i buffer ID */
    io_uring_buf_ring_add(br, bufs[i].iov_base, bufs[i].iov_len, i,
                          io_uring_buf_ring_mask(params.entries), i);
  }

  /* we've supplied buffers, make them visible to the kernel */
  io_uring_buf_ring_advance(br, params.entries);

  return (struct buffer_ring){
      .br = br,
      .bufs = bufs,
      .params = params,
  };
}

struct regbuf_pool {
    uint8_t *buf;
    struct iovec* iovecs;
    struct regbuf_freelist_entry *freelist_mem;
    struct regbuf_freelist_entry *freelist_head;
};

struct regbuf_freelist_entry {
    uint16_t bid;
    struct iovec* iov;
    struct regbuf_freelist_entry *next;
};

struct regbuf_pool regbuf_pool_init(struct io_uring *ring, size_t buf_size, size_t entries) {
    struct regbuf_freelist_entry* freelist_head = NULL;
    int res;

    uint8_t *buf = (uint8_t*) malloc(entries * buf_size);
    struct iovec *iovecs = (struct iovec*) malloc(entries * sizeof(struct iovec));
    struct regbuf_freelist_entry *freebs = (struct regbuf_freelist_entry*) malloc(entries * sizeof(struct regbuf_freelist_entry));

    for (size_t i = entries; i >= 0; i--) {
        iovecs[i].iov_base = buf + (i * buf_size);
        iovecs[i].iov_len = buf_size;
        freebs[i].bid = i;
        freebs[i].iov = &iovecs[i];
        freebs[i].next = freelist_head;
        freelist_head = &freebs[i];
    }

    res = io_uring_register_buffers(ring, iovecs, entries);
    ASSERT_POSITIVE(res);

    return (struct regbuf_pool){
        .buf = buf,
        .iovecs = iovecs,
        .freelist_mem = freebs,
        .freelist_head = freelist_head
    };
}

struct regbuf_freelist_entry *regbuf_pool_pop(struct regbuf_pool *self) {
    struct regbuf_freelist_entry *freelist_head = self->freelist_head;
    if (!freelist_head) {
        return NULL;
    }
    self->freelist_head = freelist_head->next;
    return freelist_head;
}

void regbuf_pool_put(struct regbuf_pool *self, int bid) {
    struct regbuf_freelist_entry *freebuf = self->freelist_mem + bid;
    freebuf->next = self->freelist_head;
    self->freelist_head = freebuf;
}
