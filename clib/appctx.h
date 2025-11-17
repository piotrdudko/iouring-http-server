#pragma once

#include <liburing.h>
#include <stdint.h>

#define BUFRINGS_CONN 0
#define BUFRINGS_WRITE 0
#define BUFRINGS 2

#define REGISTERED_BUFFERS 16
#define REGISTERED_BUFFER_SIZE 1024

struct bufring_init_params_t {
  uint16_t entries;
  uint16_t entry_size;
  uint16_t bgid;
};

struct bufring_t {
  struct io_uring_buf_ring *br;
  struct iovec *bufs;
  struct bufring_init_params_t params;
};

struct bufring_t buffer_ring_init(struct io_uring *ring,
                                  struct bufring_init_params_t params);

struct bufpool_t {
  uint8_t *buf;
  struct iovec *iovecs;
  struct bufpool_freebuf_t *freelist_mem;
  struct bufpool_freebuf_t *freelist_head;
};

struct bufpool_freebuf_t {
  uint16_t bid;
  struct iovec *iov;
  struct bufpool_freebuf_t *next;
};

struct bufpool_t bufpool_init(struct io_uring *ring, size_t buf_size,
                              size_t entries);
struct bufpool_freebuf_t *bufpool_pop(struct bufpool_t *self);

void bufpool_put(struct bufpool_t *self, int bid);
struct appctx_t {
  struct io_uring uring;
  struct bufpool_t bufpool;
  struct bufring_t *bufrings;
};

struct appctx_t
appctx_init(struct io_uring_params uring_params,
            struct bufring_init_params_t bufring_params[BUFRINGS],
            size_t regbuf_pool_size);
// TODO
void appctx_deinit(struct appctx_t *state);

void appctx_handle_accept(struct appctx_t *state, struct io_uring_cqe *cqe, struct msghdr *msg);
void appctx_handle_recvmsg(struct appctx_t *self, struct io_uring_cqe *cqe);
