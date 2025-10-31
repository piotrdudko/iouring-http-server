#include "logging.h"
#include "userdata.h"
#include "bufring.h"

#include <liburing.h>
#include <liburing/io_uring.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAX_CONNECTIONS 5

void run_event_loop(struct io_uring *ring,
                    struct buffer_ring bufrings[BUFRINGS],
                    struct regbuf_pool *bufpool) {
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
    case OP_WRITE_FIXED: {
      uint16_t bid = ud.fd;
      regbuf_pool_put(bufpool, bid);

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

      info(ring, bufpool, "[INFO] Packet from %s, bid: %d, len: %d, content: %.*s\n",
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
  struct io_uring_cqe *cqe;
  struct io_uring ring;

  struct io_uring_params ring_params = {.sq_thread_idle = 5000,
                                        .sq_thread_cpu = 0,
                                        .flags = IORING_SETUP_SQPOLL |
                                                 IORING_SETUP_SQ_AFF};

  ASSERT_POSITIVE(io_uring_queue_init_params(16, &ring, &ring_params));

  ASSERT_NOT_NULL(sqe = io_uring_get_sqe(&ring));
  io_uring_prep_socket(sqe, AF_INET, SOCK_STREAM, 0, 0);
  ASSERT_POSITIVE(io_uring_submit(&ring));
  ASSERT_POSITIVE(io_uring_wait_cqe(&ring, &cqe));
  int socketfd = cqe->res;
  ASSERT_POSITIVE(socketfd);

  int optval = 1;
  ASSERT_POSITIVE(setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
                   sizeof(int)));

  struct sockaddr_in serveraddr = {
      .sin_family = AF_INET,
      .sin_port = htons(3000),
      .sin_addr = {INADDR_ANY},
  };
  ASSERT_NOT_NULL(sqe = io_uring_get_sqe(&ring));
  io_uring_prep_bind(sqe, socketfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
  ASSERT_POSITIVE(io_uring_submit(&ring));
  ASSERT_POSITIVE(io_uring_wait_cqe(&ring, &cqe));

  // This doesn't work.
  // ASSERT_NOT_NULL(sqe = io_uring_get_sqe(&ring));
  // io_uring_prep_listen(sqe, socketfd, MAX_CONNECTIONS);
  // ASSERT_POSITIVE(io_uring_submit(&ring));
  // ASSERT_POSITIVE(io_uring_wait_cqe(&ring, &cqe));

  ASSERT_POSITIVE(listen(socketfd, MAX_CONNECTIONS));

  uint8_t *bufs = malloc(REGISTERED_BUFFER_SIZE * REGISTERED_BUFFERS);
  struct iovec *registered_buffers = malloc(REGISTERED_BUFFERS * sizeof(struct iovec));

  for (int i = 0; i < REGISTERED_BUFFERS; i++) {
      registered_buffers[i].iov_base = bufs + i * REGISTERED_BUFFER_SIZE;
      registered_buffers[i].iov_len = REGISTERED_BUFFER_SIZE;
  }

  res = io_uring_register_buffers(&ring, registered_buffers, REGISTERED_BUFFERS);
  ASSERT_POSITIVE(res);

  sqe = io_uring_get_sqe(&ring);
  ASSERT_NOT_NULL(sqe);
  io_uring_prep_multishot_accept(sqe, socketfd, NULL, NULL, 0);
  encode_userdata(sqe, socketfd, OP_ACCEPT);

  struct regbuf_pool bufpool = regbuf_pool_init(&ring, 16, LOGGING_BUFSIZE);

  info(&ring, &bufpool, "Server starting...");

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


  run_event_loop(&ring, buffer_rings, &bufpool);

  return 0;
}
