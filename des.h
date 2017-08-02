#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <openssl/des.h>
#include <fcntl.h>

void add_parity_bits_to_key(const uint8_t *key, uint8_t *result);
void des(uint8_t *k, uint8_t *pt, uint8_t *buf);
