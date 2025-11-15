// Include the generated bindings
#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

mod bindings;
use std::mem::MaybeUninit;

pub use bindings::*;

fn main() {
    let mut appctx = unsafe {
        let mut uring_params: io_uring_params = MaybeUninit::zeroed().assume_init();
        uring_params.sq_thread_idle = 5000;
        uring_params.sq_thread_cpu = 0;
        uring_params.flags = IORING_SETUP_SQPOLL | IORING_SETUP_SQ_AFF;

        let mut bufrings_params: [bufring_init_params_t; BUFRINGS as usize] = [
            bufring_init_params_t {
                entries: 16,
                entry_size: 1024,
                bgid: 0,
            },
            bufring_init_params_t {
                entries: 16,
                entry_size: 512,
                bgid: 1,
            },
        ];

        appctx_init(uring_params, bufrings_params.as_mut_ptr(), 16)
    };

    unsafe {
        let sqe = io_uring_get_sqe(&mut appctx.uring);
        let regbuf = *bufpool_pop(&mut appctx.bufpool);
        let log_buf = (*regbuf.iov).iov_base;
        let bid = regbuf.bid;
        io_uring_prep_read_fixed(sqe, 0, log_buf, LOGGING_BUFSIZE, 0, bid as i32);
        io_uring_submit(&mut appctx.uring);

        let mut cqe: *mut io_uring_cqe = std::ptr::null_mut();
        io_uring_wait_cqe(&mut appctx.uring, &mut cqe);
        io_uring_cqe_seen(&mut appctx.uring, cqe);

        let v = std::slice::from_raw_parts(log_buf as *const u8, (*cqe).res as usize);
        let s = std::str::from_utf8(v).unwrap();

        println!("User entered: {s}");
    }
}
