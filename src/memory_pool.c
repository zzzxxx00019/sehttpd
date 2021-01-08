#include "memory_pool.h"

#define Queue_Depth 8192
#define PoolLength Queue_Depth
#define BitmapSize PoolLength / 32
uint32_t bitmap[BitmapSize];
http_request_t *pool_ptr;

int init_memorypool()
{
    pool_ptr = calloc(PoolLength, sizeof(http_request_t));
    for (int i = 0; i < PoolLength; i++) {
        if (!&pool_ptr[i]) {
            printf("Memory %d calloc fail\n", i);
            exit(1);
        }
        (&pool_ptr[i])->pool_id = i;
    }
    return 0;
}

inline http_request_t *get_request()
{
    int pos;
    uint32_t bitset;

    for (int i = 0; i < BitmapSize; i++) {
        bitset = bitmap[i];
        if (!(bitset ^ 0xffffffff))
            continue;

        for (int k = 0; k < 32; k++) {
            if (!((bitset >> k) & 0x1)) {
                bitmap[i] ^= (0x1 << k);
                pos = 32 * i + k;
                return &pool_ptr[pos];
            }
        }
    }
    printf("Over connect!\n");
    return NULL;
}

int free_request(http_request_t *req)
{
    int pos = req->pool_id;
    bitmap[pos / 32] ^= (0x1 << (pos % 32));
    return 0;
}
