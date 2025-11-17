#include "appctx.h"
#include "logging.h"

#include <liburing.h>
#include <stdlib.h>
#include <sys/socket.h>


struct bufring_t bufring_init(struct io_uring *ring,
                              struct bufring_init_params_t params) {
  struct io_uring_buf_ring *br;
  size_t i;
  int res;

  uint8_t *_bufs = (uint8_t *)malloc(params.entries * params.entry_size);
  struct iovec *bufs =
      (struct iovec *)malloc(params.entries * sizeof(struct iovec));

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

  return (struct bufring_t){
      .br = br,
      .bufs = bufs,
      .params = params,
  };
}

struct bufpool_t bufpool_init(struct io_uring *ring, size_t buf_size,
                              size_t entries) {
  struct bufpool_freebuf_t *freelist_head = NULL;
  int res;

  uint8_t *buf = (uint8_t *)malloc(entries * buf_size);
  struct iovec *iovecs = (struct iovec *)malloc(entries * sizeof(struct iovec));
  struct bufpool_freebuf_t *freebs = (struct bufpool_freebuf_t *)malloc(
      entries * sizeof(struct bufpool_freebuf_t));

  for (size_t i = 0; i < entries; i++) {
    iovecs[i].iov_base = buf + (i * buf_size);
    iovecs[i].iov_len = buf_size;
    freebs[i].bid = i;
    freebs[i].iov = iovecs + i;
    freebs[i].next = freelist_head;
    freelist_head = freebs + i;
  }

  res = io_uring_register_buffers(ring, iovecs, entries);
  ASSERT_POSITIVE(res);

  return (struct bufpool_t){.buf = buf,
                            .iovecs = iovecs,
                            .freelist_mem = freebs,
                            .freelist_head = freelist_head};
}

struct bufpool_freebuf_t *bufpool_pop(struct bufpool_t *self) {
  struct bufpool_freebuf_t *freelist_head = self->freelist_head;
  if (!freelist_head) {
    return NULL;
  }
  self->freelist_head = freelist_head->next;
  return freelist_head;
}

void bufpool_put(struct bufpool_t *self, int bid) {
  struct bufpool_freebuf_t *freebuf = self->freelist_mem + bid;
  freebuf->next = self->freelist_head;
  self->freelist_head = freebuf;
}

struct appctx_t
appctx_init(struct io_uring_params uring_params,
            struct bufring_init_params_t bufring_params[BUFRINGS],
            size_t bufpool_size) {
  struct io_uring uring;
  ASSERT_POSITIVE(io_uring_queue_init_params(16, &uring, &uring_params));

  struct bufpool_t bufpool =
      bufpool_init(&uring, LOGGING_BUFSIZE, bufpool_size);

  struct bufring_t *bufrings = malloc(BUFRINGS * sizeof(struct bufring_t));
  for (int i = 0; i < BUFRINGS; i++) {
    bufrings[i] = bufring_init(&uring, bufring_params[i]);
  }

  return (struct appctx_t){
      .uring = uring,
      .bufpool = bufpool,
      .bufrings = bufrings,
  };
}

void appctx_handle_accept(struct appctx_t *self, struct io_uring_cqe *cqe,
                          struct msghdr *msg) {
  int clientfd = cqe->res;
  ASSERT_POSITIVE(clientfd);

  struct io_uring_sqe *sqe = io_uring_get_sqe(&self->uring);
  io_uring_prep_recvmsg_multishot(sqe, clientfd, msg, 0);
  sqe->flags |= IOSQE_BUFFER_SELECT;
  sqe->buf_group = BUFRINGS_CONN;
  encode_userdata(sqe, clientfd, OP_RECVMSG);

  format_inet_addr_from_sockfd(clientfd, fmt_addr, FMT_ADDRLEN);
  debug_log(self, "New connection from %s", fmt_addr);
}

char MESSAGE[] = "Hello, World!\n";

void appctx_handle_recvmsg(struct appctx_t *self, struct io_uring_cqe *cqe) {
  struct msghdr msg = {};
  struct userdata ud = userdata_decode(cqe);
  int bid = cqe->flags >> IORING_CQE_BUFFER_SHIFT;
  int msglen = cqe->res;
  ASSERT_POSITIVE(msglen);

  struct io_uring_recvmsg_out *out = io_uring_recvmsg_validate(
      self->bufrings[BUFRINGS_CONN].bufs[bid].iov_base, msglen, &msg);
  ASSERT_NOT_NULL(out, "recvmsg validation failed.");

  char *buf = io_uring_recvmsg_payload(out, &msg);
  int len = io_uring_recvmsg_payload_length(out, msglen, &msg);

  debug_log(self, "Packet from %s, bid: %d, len: %d, content: %.*s", fmt_addr,
            bid, msglen, len, buf);

  io_uring_buf_ring_add(
      self->bufrings[BUFRINGS_CONN].br,
      self->bufrings[BUFRINGS_CONN].bufs[bid].iov_base,
      self->bufrings[BUFRINGS_CONN].bufs[bid].iov_len, bid,
      io_uring_buf_ring_mask(self->bufrings[BUFRINGS_CONN].params.entries),
      bid);
  io_uring_buf_ring_advance(self->bufrings[BUFRINGS_CONN].br, 1);

  struct io_uring_sqe *sqe = io_uring_get_sqe(&self->uring);
  io_uring_prep_send_zc(sqe, ud.fd, MESSAGE, strlen(MESSAGE), 0, 0);
  encode_userdata(sqe, ud.fd, OP_SENDMSG);
}
