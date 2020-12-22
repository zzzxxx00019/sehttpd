#include "http.h"
#include "memory_pool.h"

struct io_uring *get_ring();
void init_io_uring();
void submit_and_wait();
//void add_accept_request(int sockfd, http_request_t *request);
void add_read_request(http_request_t *request);
void add_accept(struct io_uring *ring, int fd, struct sockaddr *client_addr, socklen_t *client_len, http_request_t *req);
void add_write_request(void *usrbuf, http_request_t *r);
void add_provide_buf(int bid);
void uring_cq_advance(int count);
void uring_queue_exit();
char *get_bufs(int bid);

