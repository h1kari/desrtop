#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>

uint64_t lfsr_step(uint64_t state) {
    uint64_t ret = 0;

    // ret[63:1] = state[62:0]
    ret |= (state & 0x7fffffffffffffff) << 1;

    // ret[0] = state[63] ^ state[62] ^ state[60] ^ state[59]
    ret |= ((state >> 63) & 1) ^ ((state >> 62) & 1) ^ ((state >> 60) & 1) ^ ((state >> 59) & 1);

    return ret;
}

uint64_t lfsr_backstep(uint64_t state) {
    uint64_t ret = 0;

    // ret[62:0] = state[63:1]
    ret |= (state & 0xfffffffffffffffe) >> 1;
    
    // ret[63] = state[0] ^ state[63] ^ state[61] ^ state[60]
    ret |= ((state & 1) ^ ((state >> 63) & 1) ^ ((state >> 61) & 1) ^ ((state >> 60) & 1)) << 63;

    return ret;
}

uint64_t lfsr64(uint64_t state) {
    uint64_t lfsr = state;
    int i;
    for(i = 0; i < 64; i++) {
        lfsr = lfsr_step(lfsr);
    }
    return lfsr;
}

uint64_t lfsr64back(uint64_t state) {
    uint64_t lfsr = state;
    int i;
    for(i = 0; i < 64; i++) {
        lfsr = lfsr_backstep(lfsr);
    }
    return lfsr;
}
