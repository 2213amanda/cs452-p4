#include "lab.h"
#include <errno.h>
#include <sys/mman.h>
 

 size_t btok(size_t bytes){
    unsigned int count = 0;
    if (bytes != 1) { bytes--; }
    while(bytes > 0){ bytes >>= 1; count++; }
    return count;
 }

 void buddy_init(struct buddy_pool *pool, size_t size){
    
    if(size == 0){ size = UINT64_C(1) << DEFAULT_K; }
    pool->kval_m = btok(size);
    pool->numbytes = UINT64_C(1) << pool->kval_m;

    pool->base = mmap(NULL, pool->numbytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pool->base == MAP_FAILED) {
        perror("buddy: could not allocate memory pool!");
    }

    for (unsigned int i = 0; i < pool->kval_m; i++){
        pool->avail[i].next = &pool->avail[i];
        pool->avail[i].prev = &pool->avail[i];
        pool->avail[i].kval = i;
        pool->avail[i].tag = BLOCK_UNUSED;
    }

    pool->avail[pool->kval_m].next = pool->base;
    pool->avail[pool->kval_m].prev = pool->base;
    struct avail *ptr = (struct avail *)pool->base;
    ptr->tag = BLOCK_AVAIL;
    ptr->kval = pool->kval_m;
    ptr->next = &pool->avail[pool->kval_m];
    ptr->prev = &pool->avail[pool->kval_m];

 }


 void buddy_destroy(struct buddy_pool *pool){
    int status = munmap(pool->base, pool->numbytes);
	if (status == -1) {
		perror("buddy: destroy failed!");
	}
 }

 struct avail *buddy_calc(struct buddy_pool *pool, struct avail *buddy) {
    uintptr_t offset = (uintptr_t)buddy - (uintptr_t)pool->base;
    uintptr_t buddy_offset = offset ^ (UINT64_C(1) << buddy->kval);
    return (struct avail *)((struct avail*)pool->base + buddy_offset);

 }


 void *buddy_malloc(struct buddy_pool *pool, size_t size) {
    if (!pool || size == 0){
        return NULL;
    }

    size += sizeof(struct avail);

    size_t kval = btok(size);
    if(kval < SMALLEST_K) kval = SMALLEST_K;

    for (size_t k = kval; k<= pool->kval_m; k++){
        if(pool->avail[k].next != &pool->avail[k]){
            struct avail *block = pool->avail[k].next;
            block->prev->next = block->next;
            block->next->prev = block->prev;

            while (k > kval){
                k--;
                uintptr_t buddy_offset = (uintptr_t)block + (UINT64_C(1) << k);
                struct avail *split_buddy = (struct avail *)(buddy_offset);
                split_buddy->kval = k;
                split_buddy->tag = BLOCK_AVAIL;
                split_buddy->next = pool->avail[k].next;
                split_buddy->prev = &pool->avail[k]; 
                pool->avail[k].next->prev = split_buddy;
                pool->avail[k].next = split_buddy;
            }

            block->tag = BLOCK_RESERVED;
            return (void *)((uintptr_t)block + sizeof(struct avail));
        }
    }
    errno = ENOMEM;
    return NULL;
 }

 void buddy_free(struct buddy_pool *pool, void *ptr) {
    if (!ptr || !pool){
        return;
    }

    struct avail *block = (struct avail *)((uintptr_t)ptr - sizeof(struct avail));
    block->tag = BLOCK_AVAIL;

    while (block->kval < pool->kval_m) {
        struct avail *buddy = buddy_calc(pool, block);
        if (buddy->tag != BLOCK_AVAIL || buddy->kval != block->kval) break;

        buddy->prev->next = buddy->next;
        buddy->next->prev = buddy->prev;

        if (buddy < block) block = buddy;
        block->kval++;
    }

    block->next = pool->avail[block->kval].next;
    block->prev = &pool->avail[block->kval];
    pool->avail[block->kval].next->prev = block;
    pool->avail[block->kval].next = block;
 }


 void *buddy_realloc(struct buddy_pool *pool, void *ptr, size_t size) {
    if (!ptr) {
        return buddy_malloc(pool, size);
    }

    if(size  == 0) {
        buddy_free(pool, ptr);
        return NULL;
    }

    struct avail *block = (struct avail *)ptr - 1;
    size_t old_size = UINT64_C(1) << block->kval;

    if(size <= old_size - sizeof(struct avail)){
        return ptr;
    }

    void *new_block = buddy_malloc(pool, size);
    if (new_block){
        size_t copy_size = old_size - sizeof(struct avail);
        memmove(new_block, ptr, copy_size);
        buddy_free(pool, ptr);
    }
    return new_block;
 }

 int myMain(int argc, char** argv) {
    return 0;
 }