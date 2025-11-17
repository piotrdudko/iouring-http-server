#include "../clib/appctx.h"
#include "../clib/logging.h"
#include "../clib/userdata.h"

#include <liburing.h>
#include <liburing/io_uring.h>
#include <netinet/in.h>
#include <stdatomic.h>
#include <sys/socket.h>

#define MAX_CONNECTIONS 5

void run_event_loop(struct appctx_t *appctx) {
  int res;
  struct msghdr msg = {};
  struct io_uring_cqe *cqe;

  for (;;) {
    res = io_uring_wait_cqe(&appctx->uring, &cqe);
    ASSERT_POSITIVE(res);

    struct userdata ud = userdata_decode(cqe);
    switch (ud.op) {
    case OP_LISTEN: {
      break;
    }
    case OP_WRITE: {
      break;
    }
    case OP_WRITE_FIXED: {
      uint16_t bid = ud.fd;
      bufpool_put(&appctx->bufpool, bid);
      break;
    }
    case OP_ACCEPT: {
      appctx_handle_accept(appctx, cqe, &msg);
      break;
    }
    case OP_RECVMSG: {
      appctx_handle_recvmsg(appctx, cqe);
      break;
    }
    default: {
      break;
    }
    }
    io_uring_cqe_seen(&appctx->uring, cqe);

    res = io_uring_submit(&appctx->uring);
    ASSERT_POSITIVE(res);
  }
}

int main() {
  int res;
  struct io_uring_sqe *sqe;

  struct io_uring_params uring_params = {.sq_thread_idle = 5000,
                                         .sq_thread_cpu = 0,
                                         .flags = IORING_SETUP_SQPOLL |
                                                  IORING_SETUP_SQ_AFF};

  struct bufring_init_params_t bufrings_params[BUFRINGS] = {
      {.entries = 16, .entry_size = 1024, .bgid = 0},
      {.entries = 16, .entry_size = 512, .bgid = 1},
  };

  struct appctx_t appctx = appctx_init(uring_params, bufrings_params, 16);

  int socketfd = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_POSITIVE(socketfd);

  int optval = 1;
  ASSERT_POSITIVE(setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR,
                             (const void *)&optval, sizeof(int)));

  struct sockaddr_in serveraddr = {
      .sin_family = AF_INET,
      .sin_port = htons(3000),
      .sin_addr = {INADDR_ANY},
  };
  ASSERT_POSITIVE(
      bind(socketfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)));
  ASSERT_POSITIVE(listen(socketfd, MAX_CONNECTIONS));

  sqe = io_uring_get_sqe(&appctx.uring);
  ASSERT_NOT_NULL(sqe);
  io_uring_prep_multishot_accept(sqe, socketfd, NULL, NULL, 0);
  encode_userdata(sqe, socketfd, OP_ACCEPT);

  debug_log(&appctx, "Server starting 1...");
  info_log(&appctx, "Server starting 2...");
  warn_log(&appctx, "Server starting 3...");
  error_log(&appctx, "Server starting 4...");

  res = io_uring_submit(&appctx.uring);
  ASSERT_POSITIVE(res);

  run_event_loop(&appctx);

  return 0;
}
