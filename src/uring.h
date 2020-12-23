#include "http.h"
#include "memory_pool.h"

#define Queue_Depth 8192

#define accept 0
#define read 1
#define write 2
#define prov_buf 3
#define uring_timer 4

struct io_uring *get_ring();
void init_io_uring();
void submit_and_wait();
void add_read_request(http_request_t *request);
void add_accept(struct io_uring *ring,
                int fd,
                struct sockaddr *client_addr,
                socklen_t *client_len,
                http_request_t *req);
void add_write_request(void *usrbuf, http_request_t *r);
void add_provide_buf(int bid);
void uring_cq_advance(int count);
void uring_queue_exit();
void *get_bufs(int bid);
