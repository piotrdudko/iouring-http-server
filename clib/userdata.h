#pragma once

#include <liburing.h>

struct userdata {
  union {
    struct {
      uint32_t fd;
      uint8_t op;
    };
    uint64_t val;
  };
};

enum opcode {
  OP_LISTEN = 1,
  OP_ACCEPT = 2,
  OP_RECVMSG = 3,
  OP_WRITE = 4,
  OP_WRITE_FIXED = 5,
  OP_SENDMSG = 6,
};

void encode_userdata(struct io_uring_sqe *sqe, uint16_t fd, uint8_t op);
struct userdata userdata_decode(struct io_uring_cqe *cqe);
