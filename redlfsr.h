#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>

uint64_t lfsr_step(uint64_t state);
uint64_t lfsr_backstep(uint64_t state);
uint64_t lfsr64(uint64_t state);
uint64_t lfsr64back(uint64_t state);
