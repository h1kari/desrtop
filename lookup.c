#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <inttypes.h>
#include <pthread.h>

#include "lookup.h"
#include "op.h"

pthread_t lookup_threads[LU_NUM_THREADS];
pthread_mutex_t stdout_mutex;

// set to 1 when parent is ready to exit
int lookup_exit = 0;

// set to 1 to print debug info
int debug = 0;

// queue for all of the threads
struct queue_s queue[LU_NUM_THREADS];

uint64_t lu_off = 0;

// return next lookup job for this thread
// returns 0 if there's no data to use
static int lookup_pop(int thread, struct lookup_s *r) {

    pthread_mutex_lock(&queue[thread].mutex);
    if(queue[thread].i == 0) {
        pthread_mutex_unlock(&queue[thread].mutex);
        return 0;
    }
    queue[thread].i--;
    // make copy of lookup entry in case it changes out from underneath us
    *r = queue[thread].q[queue[thread].i];
    pthread_mutex_unlock(&queue[thread].mutex);

    return 1;
}

uint8_t sector_buf[LU_NUM_THREADS][LU_SECTOR_SIZE];
static void lookup_vec(int thread, struct lookup_s *p, int fd) {
    uint64_t sector_addr, ep_verify, *v, sp;
    uint8_t *buf = sector_buf[thread];

    sector_addr  = p->ep / LU_SECTOR_DIV;
    sector_addr *= LU_SECTOR_SIZE;
    ep_verify    = p->ep & LU_EP_VERIFY_MASK;

    if(lseek(fd, lu_off + sector_addr, SEEK_SET) < 0)
        return;

    if(read(fd, buf, LU_SECTOR_SIZE) != LU_SECTOR_SIZE)
        return;

    for(int i = 0; i < LU_SECTOR_SIZE; i += 8) {
        v = (uint64_t *)&buf[i];
        if(*v == 0) break;

        if((*v & LU_EP_VERIFY_MASK) == ep_verify) {
            sp = *v >> LU_SP_SHIFT;
            op2(p->off, sp, p->n);
        }
    }
}

// thread loop
char *lu_file;
static void *lookup_loop(void *arg) {
    int thread = *(int *)arg;
    struct lookup_s p;
    int fd;

    if((fd = open(lu_file, O_RDONLY)) < 0) {
        perror("unable to open file!");
        exit(1);
    }
    
    if(debug) {
    pthread_mutex_lock(&stdout_mutex);
    printf("thread %d started\n", thread);
    pthread_mutex_unlock(&stdout_mutex);
    }

    while(1) {
        if(!lookup_pop(thread, &p)) {
            if(lookup_exit) pthread_exit(NULL);
            continue;
        }

        // lookup value!
        lookup_vec(thread, &p, fd);
    }

    pthread_exit(NULL);
}

// create our threads
int threads[LU_NUM_THREADS];
void lookup_create(char *file, int off) {
    lu_file = file;
    lu_off = off ? LU_PART_DIV * 4096UL : 0UL;
    for(int i = 0; i < LU_NUM_THREADS; i++) {
        queue[i].i = 0;
        threads[i] = i;
        if(pthread_create(&lookup_threads[i], NULL, lookup_loop, &threads[i])) {
            perror("error creating lookup thread!");
            exit(1);
        }
    }
}

void lookup_destroy(void) {
    // signal threads to exit!
    lookup_exit = 1;

    // wait for them all to exit
    for(int i = 0; i < LU_NUM_THREADS; i++) {
        pthread_join(lookup_threads[i], NULL);
    }
}

// push new lookup into the queue
static void lookup_push(int thread, uint64_t off, uint64_t ep, int n) {
    struct lookup_s *p;

    pthread_mutex_lock(&queue[thread].mutex);
    if(queue[thread].i >= LU_QUEUE_DEPTH-1) {
        fprintf(stderr, "queue depth overflow!\n");
        exit(1);
    }
    p = &queue[thread].q[queue[thread].i];
    p->off = off;
    p->ep  = ep;
    p->n   = n;
    queue[thread].i++;
    pthread_mutex_unlock(&queue[thread].mutex);
}

void lookup(uint64_t off, uint64_t ep, int n) {
    int min = LU_QUEUE_DEPTH, thread = 0;

    // find thread with smallest queue
    for(int i = 0; i < LU_NUM_THREADS; i++) {
        if(queue[i].i < min) {
            min = queue[i].i;
            thread = i;
        }
    }

    // then insert into that thread's queue
    if(debug) {
    pthread_mutex_lock(&stdout_mutex);
    printf("** %d : %016lx %016lx\n", thread, off, ep);
    pthread_mutex_unlock(&stdout_mutex);
    }
    lookup_push(thread, off, ep, n);
}
