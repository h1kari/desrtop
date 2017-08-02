#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <openssl/des.h>
#include <fcntl.h>

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

void des(uint8_t *k, uint8_t *pt, uint8_t *buf) {
    uint8_t parity[8], iv[8];
    DES_key_schedule ks1;

    memset(iv, 0, 8);

    add_parity_bits_to_key(k, parity);
    DES_set_key((const_DES_cblock *)&parity, &ks1);
    DES_ncbc_encrypt(pt, buf, 8, &ks1, (DES_cblock *)iv, DES_ENCRYPT);

    return;
}
