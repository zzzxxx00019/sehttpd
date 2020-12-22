#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <http.h>

int init_memorypool();
http_request_t *get_request();
int free_request(http_request_t *req);

