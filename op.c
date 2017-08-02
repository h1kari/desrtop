#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <openssl/des.h>

#include "op.h"
#include "lookup.h"
#include "redlfsr.h"

// A pair is a pointer to a Pico device and a stream
PicoDrv *drv;
int stream;

static void fsl_reset() {
    uint32_t w128[4];
    w128[3] = w128[2] = w128[1] = w128[0] = 0xffffffffULL;
    drv->WriteStream(stream, w128, 16);
    usleep(500000);
}

#define RESETBUF_MAX 1024
static void stream_reset() {
    int read_avail;
    while((read_avail = drv->GetBytesAvailable(stream, true)) != 0) {
        uint32_t resetbuf[RESETBUF_MAX];

        for(int i = 0; i < read_avail; i += RESETBUF_MAX) {
            int read_amount = read_avail < RESETBUF_MAX*4 ? read_avail : RESETBUF_MAX*4;
            if(read_amount < 16) break;
            if(drv->ReadStream(stream, resetbuf, read_amount) != read_amount) {
                fprintf(stderr, "error reading from stream!");
                exit(1);
            }
        }
    }
}

static void fpga_reset() {
    stream_reset();
    fsl_reset();
    stream_reset();
    usleep(500000);
}

static void fpga_create(int fpga, int reboot) {
    char ibuf[1024];
    int err;

    drv = new PICODRV(fpga);

    if(reboot)
        drv->LoadFPGA("M510-DES-op.bit");

    if(err = drv->GetError()) {
        fprintf(stderr, "Error loading FPGA: %s\n", PicoErrors_FullError(stream, ibuf, sizeof(ibuf)));
        exit(1);
    }

    if((stream = drv->CreateStream(1)) < 0) {
        fprintf(stderr, "CreateStream error: %s\n", PicoErrors_FullError(stream, ibuf, sizeof(ibuf)));
        exit(1);
    }

    fpga_reset();
}

// op parameters set on creation
uint64_t *op_cts, op_redux, op_pt;
int op_cts_n;
uint32_t op_t;

// Yanked from http://www.exampledepot.com/egs/javax.crypto/MakeDes.html
// Keeps track of the bit position in the result
static void add_parity_bits_to_key(const uint8_t *key, uint8_t *result)
{
    int result_idx = 1;
    int bit_count = 0; // Number of 1 bits in each 7-bit chunk
    int i;

    memset(result, 0, 8);

    // Process each of the 56 bits
    for (i=0; i<56; i++)
    {   
        if (key[6-i/8] & (1<<(i%8)))
        {   
            result[7-result_idx/8] |= (1<<(result_idx%8)) & 0xFF;
            ++bit_count;
        }

        // Set the parity bit after every 7 bits
        if ((i+1) % 7 == 0)
        {   
            if (bit_count % 2 == 0)
            {   
                // Set low-order bit (parity bit) if bit count is even
                result[7-result_idx/8] |= 1;
            }
            ++result_idx;
            bit_count = 0;
        }
        ++result_idx;
    }
}

static void des(uint64_t key, uint64_t pt, uint64_t *r) {
    uint8_t parity[8], iv[8], k[7], ptc[8], out[8];
    uint64_t ret;
    DES_key_schedule ks1;

    // have to reverse key for some reason..
    for(int i = 0; i < 7; i++)
        k[i] = (key >> (uint64_t)((6-i) * 8)) & 0xff;

    // and have to reverse pt
    for(int i = 0; i < 8; i++)
        ptc[i] = (pt >> (uint64_t)((7-i) * 8)) & 0xff;

    memset(iv, 0, 8);

    add_parity_bits_to_key(k, parity);
    DES_set_key((const_DES_cblock *)&parity, &ks1);
    DES_ncbc_encrypt(ptc, out, 8, &ks1, (DES_cblock *)iv, DES_ENCRYPT);

    ret = 0UL;
    for(int i = 0; i < 8; i++)
        ret |= (uint64_t)out[i] << (uint64_t)((7-i) * 8);

    *r = ret;
}

static uint64_t gensp(uint64_t sp) {
    uint64_t r = 0;

    r = sp;
    r ^= ~(sp << 5UL);
    r ^= sp << 10UL;
    r ^= ~(sp << 15UL);
    r ^= (sp << 20UL) | (sp >> 36UL);
    r ^= ~((sp << 25UL) | (sp >> 31UL));
    r ^= (sp << 31UL) | (sp >> 25UL);
    r ^= ~((sp << 36UL) | (sp >> 20UL));
    r ^= (sp << 41UL) | (sp >> 15UL);
    r ^= ~((sp << 46UL) | (sp >> 10UL));
    r ^= (sp << 51UL) | (sp >> 5UL);

    r &= 0xffffffffffffffUL;

    return r;
}

uint32_t opwrbuf[OPWRBUF_LEN] __attribute__((aligned(16)));
int opwrbuf_idx = 0;
int op_wrcount = 0;
int op_wrcount2 = 0;
int op1_wrcount = 0;
int op2_wrcount = 0;

uint32_t oprdbuf[OPRDBUF_LEN] __attribute__((aligned(16)));
int oprdbuf_idx = 0;
int op_rdcount = 0;
int op_exit = 0;
int op1_rdcount = 0;
int op2_rdcount = 0;

pthread_mutex_t op_mutex;

static void op_flush() {
    if(opwrbuf_idx < 8)
        return;

    //printf("writing stream %d %d %d\n", op_wrcount, opwrbuf_idx, op_wrcount2);
    drv->WriteStream(stream, opwrbuf, opwrbuf_idx * sizeof(uint32_t));
    op_wrcount2 += opwrbuf_idx;

    opwrbuf_idx = 0;
}

// push op vector job into queue to be run on fpga
static void op_push(uint32_t id, uint32_t off, uint64_t redux, uint64_t ct) {
    char ibuf[1024];

    opwrbuf[opwrbuf_idx+0] = ct & 0xffffffff;
    opwrbuf[opwrbuf_idx+1] = (ct >> 32UL) & 0xffffffff;
    opwrbuf[opwrbuf_idx+2] = 0;
    opwrbuf[opwrbuf_idx+3] = 0;
    opwrbuf[opwrbuf_idx+4] = redux & 0xffffffff;
    opwrbuf[opwrbuf_idx+5] = (redux >> 32UL) & 0xffffffff;
    opwrbuf[opwrbuf_idx+6] = (id << 20) | (off & 0xfffff);
    opwrbuf[opwrbuf_idx+7] = 0x80000000;
    op_wrcount++;
    opwrbuf_idx += 8;

    if((id & 3) == OP1_ID) op1_wrcount++;
    if((id & 3) == OP2_ID) op2_wrcount++;

    if(opwrbuf_idx < OPWRBUF_LEN)
        return;

    op_flush();
}

static void op_process(uint32_t id, uint32_t off, uint64_t k) {
    int n = id >> 2;
    if((id & 3) == OP1_ID) {
        lookup(off+1, k, n);
        op1_rdcount++;
    } else if((id & 3) == OP2_ID) {
        uint64_t ret;
        des(k, op_pt, &ret);
        if(ret == op_cts[n]) {
            printf("*** FOUND KEY %014lx ***\n", k);
            fflush(stdout);
        }
        op2_rdcount++;
    }
}

static void stat_print(void) {
    static uint32_t stat_last[4];
    uint32_t stat[4];
    drv->ReadDevice(0, stat, 16);
    if(memcmp(stat, stat_last, 16) != 0) {
        printf("   s1i_count: %u\n", stat[0]);
        printf("   s1o_count: %u\n", stat[1]);
        printf("   fsl_count: %u\n", stat[2]);
        printf("   op_wrcount2: %u\n", op_wrcount2);
        printf("   fsl_in_full=%d ring_in_full=%d full_latch=%d stop_clock_latch=%d\n", stat[3] & 1, (stat[3] >> 1) & 1, (stat[3] >> 2) & 1, (stat[3] >> 3) & 1);
        memcpy(stat_last, stat, 16);
    }
}

static void *op_poll(void *arg) {
    while(1) {
        int read_avail = drv->GetBytesAvailable(stream, true);
        int read_max   = OPRDBUF_LEN;
        int read_len   = read_avail > read_max ? read_max : read_avail;

        if(read_len < 16) continue;

        if(drv->ReadStream(stream, oprdbuf, read_len) != read_len) {
            fprintf(stderr, "error reading from stream!");
            exit(1);
        }

        for(int i = 0; i < read_len/4; i += 4) {
            uint64_t k   = *(uint64_t *)&oprdbuf[i+0];
            uint32_t off = oprdbuf[i+2] & 0xfffff;
            uint32_t id  = (oprdbuf[i+2] >> 20) & 0x7ff;

            op_process(id, off, k);
            op_rdcount++;
        }

        if(op_exit && (op_rdcount == op_wrcount)) {
            pthread_exit(NULL);
        }
    }
}

pthread_t op_poll_thread;
uint64_t op2_redux;
void op_create(int fpga, int reboot, uint64_t *cts, int cts_n, uint64_t redux, uint32_t t, uint64_t pt) {
    // initialize our FPGA
    fpga_create(fpga, reboot);

    // set op parameters
    op_cts = cts;
    op_cts_n = cts_n;
    op_redux = redux;
    op2_redux = lfsr64(redux);
    op_t = t;
    op_pt = pt;

    opwrbuf_idx = 0;
    op_wrcount = 0;
    op_wrcount2 = 0;

    oprdbuf_idx = 0;
    op_rdcount = 0;
    op_exit = 0;

    op1_rdcount = 0;
    op1_wrcount = 0;

    // start up our polling thread
    opwrbuf_idx = oprdbuf_idx = 0;
    pthread_create(&op_poll_thread, NULL, op_poll, NULL);
}

void op_destroy() {
    op_exit = 1;
    pthread_join(op_poll_thread, NULL);
}

static uint64_t red(uint64_t ct, uint64_t red) {
    return (ct ^ red) & 0xffffffffffffffUL;
}

void op1() {
    uint64_t redux;

    for(int j = 0; j < op_cts_n; j++) {

        redux = op_redux;
        for(int i = 0; i < op_t; i++) {
            redux = lfsr64(redux);
        }

        for(int i = 0; i < op_t; i++) {
            if(i > OP1_THRESHOLD) {
                pthread_mutex_lock(&op_mutex);
                op_push((j << 2) | OP1_ID, i-1, redux, op_cts[j]);
                pthread_mutex_unlock(&op_mutex);
            }

            redux = lfsr64back(redux);
        }
    }

    pthread_mutex_lock(&op_mutex);
    op_flush();
    pthread_mutex_unlock(&op_mutex);
}

void op1_done() {
    while(op1_rdcount != op1_wrcount) {
        usleep(10000);
    }
}

void op2_done() {
    while(op2_rdcount != op2_wrcount) {
        usleep(10000);
    }
}

void op2(uint32_t off, uint64_t sp, int n) {
    uint32_t t = (op_t-1)-off;

    if(t > OP2_THRESHOLD) {
        uint64_t key = gensp(sp);
        uint64_t ct;

        des(key, op_pt, &ct);

        pthread_mutex_lock(&op_mutex);
        op_push((n << 2) | OP2_ID, t-2, op2_redux, ct);
        pthread_mutex_unlock(&op_mutex);
    }

    pthread_mutex_lock(&op_mutex);
    op_flush();
    pthread_mutex_unlock(&op_mutex);
}
