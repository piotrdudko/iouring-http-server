#include "userdata.h"
#include <liburing.h>

 void encode_userdata(struct io_uring_sqe *sqe,uint16_t fd, uint8_t op) {
  struct userdata ud = {.fd = fd, .op = op, };

  io_uring_sqe_set_data64(sqe, ud.val);
}

struct userdata userdata_decode(struct io_uring_cqe *cqe) {
  return (struct userdata){.val = cqe->user_data};
}
