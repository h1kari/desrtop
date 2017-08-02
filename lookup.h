#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <inttypes.h>
#include <pthread.h>

#define LU_NUM_THREADS 8
#define LU_QUEUE_DEPTH 65536

// contains a single lookup vector
struct lookup_s {
    uint64_t off;
    uint64_t ep;
    int n;
};

// queue for a single thread
struct queue_s {
    int i;
    pthread_mutex_t mutex;
    struct lookup_s q[LU_QUEUE_DEPTH];
};

#define LU_SECTOR_DIV      (18010713UL*4UL)
#define LU_PART_DIV        (125025579UL)
#define LU_EP_VERIFY_MASK  (0x7ffffffUL)
#define LU_SP_SHIFT        (27UL)
#define LU_SECTOR_SIZE     (512UL)

void lookup_create(char *file, int off);
void lookup_destroy(void);
void lookup(uint64_t off, uint64_t ep, int n);
