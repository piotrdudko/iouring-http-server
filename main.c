#include "logging.h"
#include "userdata.h"

#include <fcntl.h>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CONNECTIONS 5

#define BUFRINGS_CONN 0
#define BUFRINGS_WRITE 0
#define BUFRINGS 2

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

  uint8_t *_bufs = malloc(params.entries * params.entry_size);
  struct iovec *bufs = malloc(params.entries * sizeof(struct iovec));

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

void run_event_loop(struct io_uring *ring,
                    struct buffer_ring bufrings[BUFRINGS]) {
  int res;
  struct io_uring_cqe *cqe;
  struct io_uring_sqe *sqe;
  struct msghdr msg = {};

  for (;;) {
    res = io_uring_wait_cqe(ring, &cqe);
    ASSERT_POSITIVE(res);

    struct userdata ud = userdata_decode(cqe);
    switch (ud.op) {
    case OP_WRITE: {
      break;
    }
    case OP_ACCEPT: {
      int clientfd = cqe->res;
      ASSERT_POSITIVE(clientfd);

      sqe = io_uring_get_sqe(ring);
      io_uring_prep_recvmsg_multishot(sqe, clientfd, &msg, 0);
      sqe->flags |= IOSQE_BUFFER_SELECT;
      sqe->buf_group = 0;
      encode_userdata(sqe, clientfd, OP_RECVMSG);

      format_inet_addr_from_sockfd(clientfd, fmt_addr, FMT_ADDRLEN);
      break;
    }
    case OP_RECVMSG: {
      int bid = cqe->flags >> IORING_CQE_BUFFER_SHIFT;
      int msglen = cqe->res;
      ASSERT_POSITIVE(msglen);

      struct io_uring_recvmsg_out *out = io_uring_recvmsg_validate(
          bufrings[BUFRINGS_CONN].bufs[bid].iov_base, msglen, &msg);
      ASSERT_NOT_NULL(out, "recvmsg validation failed.");

      char *buf = io_uring_recvmsg_payload(out, &msg);
      int len = io_uring_recvmsg_payload_length(out, msglen, &msg);

      info(ring, "[INFO] Packet from %s, bid: %d, len: %d, content: %.*s\n",
           fmt_addr, bid, msglen, len, buf);

      io_uring_buf_ring_add(
          bufrings[BUFRINGS_CONN].br,
          bufrings[BUFRINGS_CONN].bufs[bid].iov_base,
          bufrings[BUFRINGS_CONN].bufs[bid].iov_len, bid,
          io_uring_buf_ring_mask(bufrings[BUFRINGS_CONN].params.entries), bid);
      io_uring_buf_ring_advance(bufrings[BUFRINGS_CONN].br, 1);

      break;
    }
    default: {
      break;
    }
    }
    io_uring_cqe_seen(ring, cqe);

    res = io_uring_submit(ring);
    ASSERT_POSITIVE(res);
  }
}

int main() {
  int res;
  struct io_uring_sqe *sqe;
  struct io_uring ring;

  int socketfd = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_POSITIVE(socketfd);

  int optval = 1;
  res = setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
                   sizeof(int));
  ASSERT_POSITIVE(res);

  struct sockaddr_in serveraddr = {
      .sin_family = AF_INET,
      .sin_port = htons(3000),
      .sin_addr = {INADDR_ANY},
  };
  res =
      bind(socketfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr));
  ASSERT_POSITIVE(res);

  res = listen(socketfd, MAX_CONNECTIONS);
  ASSERT_POSITIVE(res);

  struct io_uring_params ring_params = {.sq_thread_idle = 5000,
                                        .sq_thread_cpu = 0,
                                        .flags = IORING_SETUP_SQPOLL |
                                                 IORING_SETUP_SQ_AFF};

  res = io_uring_queue_init_params(16, &ring, &ring_params);
  ASSERT_POSITIVE(res);

  sqe = io_uring_get_sqe(&ring);
  ASSERT_NOT_NULL(sqe);
  io_uring_prep_multishot_accept(sqe, socketfd, NULL, NULL, 0);
  encode_userdata(sqe, socketfd, OP_ACCEPT);

  info(&ring, "Server starting...");

  res = io_uring_submit(&ring);
  ASSERT_POSITIVE(res);

  struct buffer_ring buffer_rings[BUFRINGS] = {
      buffer_ring_init(&ring,
                       (struct buffer_ring_init_params){
                           .entries = 16, .entry_size = 1024, .bgid = 0}),
      buffer_ring_init(&ring,
                       (struct buffer_ring_init_params){
                           .entries = 16, .entry_size = 512, .bgid = 1}),
  };

  run_event_loop(&ring, buffer_rings);

  return 0;
}
