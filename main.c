#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <picodrv.h>
#include <fcntl.h>
#include <pthread.h>

#include "op.h"
#include "lookup.h"

void usage(char *progname) {
    fprintf(stderr, "usage: %s -c <ct> -t <tbl> -f <fpga> -r -x\n", progname);
    exit(1);
}

#define TBL_MIN 1
#define TBL_MAX 12
uint64_t tbl2redux[] = {
    0x519fdbc8487517fdUL,
    0x91195518a7eb9f0dUL,
    0xb77c44899d57182aUL,
    0xe4ac4df8f798d66dUL,
    0xb661a4b42b0bac79UL,
    0x42dd1896b6110fb3UL,
    0x33f8fb21ce9d66e2UL,
    0x84b37c5b2b988894UL,
    0x4c2f46ba44b30d07UL,
    0xc2049576b2927b13UL,
    0x0f6d8d3c1567de61UL,
    0x85e8f86e985b6eabUL
};

int main(int argc, char *argv[]) {
    int tbl = 1, run = 0, fpga = 1, reboot = 0, then_exit = 0;
    uint64_t redux, pt = 0x1122334455667788;
    static uint64_t cts[16];
    int cts_n = 0;
    uint32_t t = 500000;
    int off = 0;
    char lu_file[10];
    char *endptr, *ptr;

    // parse arguments
    int opt;
    while((opt = getopt(argc, argv, "c:t:f:rox")) != -1) {
        switch(opt) {
            case 'c':
                ptr = optarg;
                while(1) {
                    cts[cts_n] = strtoul(ptr, &endptr, 16);
                    if(ptr == endptr) break;
                    cts_n++;
                    ptr = endptr + 1;
                }
                run = 1;
                break;
            case 't':
                tbl = atoi(optarg);
                break;
            case 'f':
                fpga = atoi(optarg);
                break;
            case 'r':
                reboot = 1;
                break;
            case 'x':
                then_exit = 1;
                break;
            case 'o':
                off = 1;
                break;
            default:
                usage(argv[0]);
                break;
        }
    }

    if((!run && !then_exit) || (tbl < TBL_MIN) || (tbl > TBL_MAX))
        usage(argv[0]);

    sprintf(lu_file, "%d.dat", tbl);

    redux = tbl2redux[tbl-1];

    // initialize our op core
    op_create(fpga, reboot, cts, cts_n, redux, t, pt);

    if(then_exit) exit(0);

    // create lookup threads
    lookup_create(lu_file, off);

    // submit op1 values
    //printf("submitting op1 values..\n");
    //fflush(stdout);
    op1();

    // wait until op1 values have all moved to lookup
    //printf("waiting for op1 results..");
    //fflush(stdout);
    op1_done();
    //printf("done.\n");
    //fflush(stdout);

    // then signal lookup that all values have been submitted and wait for threads to complete
    //printf("waiting for lookup to finish..");
    //fflush(stdout);
    lookup_destroy();
    //printf("done.\n");
    //fflush(stdout);

    // then wait for all op2 jobs to complete
    //printf("waiting for op2 results to finish..");
    //fflush(stdout);
    op2_done();
    //printf("done.\n");
    //op_destroy();

    //printf("done!\n");
    //fflush(stdout);
    exit(0);
}

