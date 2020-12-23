#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <liburing.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"
#include "logger.h"
#include "memory_pool.h"
#include "uring.h"

/* the length of the struct epoll_events array pointed to by *events */
//#define MAXEVENTS 1024
#define LISTENQ 1024

#define accept 0
#define read 1
#define write 2
#define prov_buf 3
#define uring_timer 4

static int open_listenfd(int port)
{
    int listenfd, optval = 1;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminate "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &optval,
                   sizeof(int)) < 0)
        return -1;

    /* Listenfd will be an endpoint for all requests to given port. */
    struct sockaddr_in serveraddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons((unsigned short) port),
        .sin_zero = {0},
    };
    if (bind(listenfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;

    return listenfd;
}
/* TODO: use command line options to specify */
#define PORT 8081
#define WEBROOT "./www"

int main()
{
    int listenfd = open_listenfd(PORT);
    init_memorypool();
    init_io_uring();
    struct io_uring *ring = get_ring();

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    http_request_t *req = get_request();
    add_accept(ring, listenfd, (struct sockaddr *) &client_addr, &client_len,
               req);

    printf("Web server started.\n");

    while (1) {
        submit_and_wait();
        struct io_uring_cqe *cqe;
        unsigned head;
        unsigned count = 0;
        io_uring_for_each_cqe(ring, head, cqe)
        {
            ++count;
            http_request_t *cqe_req = io_uring_cqe_get_data(cqe);
            int type = cqe_req->event_type;

            if (type == accept) {
                add_accept(ring, listenfd, (struct sockaddr *) &client_addr,
                           &client_len, cqe_req);

                int clientfd = cqe->res;
                if (clientfd >= 0) {
                    http_request_t *request = get_request();
                    init_http_request(request, clientfd, WEBROOT);
                    add_read_request(request);
                }
            } else if (type == read) {
                int read_bytes = cqe->res;
                if (read_bytes <= 0) {
                    int ret = http_close_conn(cqe_req);
                    assert(ret == 0 && "http_close_conn");
                } else {
                    cqe_req->bid = (cqe->flags >> IORING_CQE_BUFFER_SHIFT);
                    do_request(cqe_req, read_bytes);
                }
            } else if (type == write) {
                add_provide_buf(cqe_req->bid);
                int write_bytes = cqe->res;
                if (write_bytes <= 0) {
                    int ret = http_close_conn(cqe_req);
                    assert(ret == 0 && "http_close_conn");
                } else {
                    add_read_request(cqe_req);
                }
            } else if (type == prov_buf) {
                free_request(cqe_req);
            } else if (type == uring_timer) {
                free_request(cqe_req);
            }
        }
        uring_cq_advance(count);
    }
    uring_queue_exit();

    return 0;
}
