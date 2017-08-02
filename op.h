#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <picodrv.h>
#include <fcntl.h>
#include <pthread.h>

#include "redlfsr.h"

// must be at least 0
#define OP1_THRESHOLD 0 //1000
#define OP2_THRESHOLD 1000
#define OP1_ID 0x1
#define OP2_ID 0x2

#define OPWRBUF_LEN (1024*8)
#define OPRDBUF_LEN (1024*4)

void op_create(int fpga, int reboot, uint64_t *cts, int cts_n, uint64_t redux, uint32_t t, uint64_t pt);
void op_destroy();
void op1();
void op1_done();
void op2(uint32_t off, uint64_t sp, int n);
void op2_done();
