#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define WINDOW_SIZE 4096
#define SEGSIZE 524288

static uint8_t seed_dictionary[WINDOW_SIZE];
static uint8_t plain[WINDOW_SIZE + SEGSIZE];
static uint8_t comp[WINDOW_SIZE + (SEGSIZE * 3 / 2)]; // 150% guard size

void lzss_prepare_dictionary(uint8_t *a_seed, size_t a_seed_len)
{
    memset(seed_dictionary, 0, WINDOW_SIZE);
    memcpy(seed_dictionary + WINDOW_SIZE - a_seed_len, a_seed, a_seed_len);
    printf("Copied seed_len %ld string to seed_dictionary buffer at position %ld.\n", a_seed_len, WINDOW_SIZE - a_seed_len);
}

void lzss_encode(uint8_t *a_in, size_t a_in_len, uint8_t *a_out)
{
    size_t window_ptr = WINDOW_SIZE; // start at 4096
    memcpy(a_in, seed_dictionary, WINDOW_SIZE);
}

void lzss_decode(uint8_t *a_in, size_t a_in_len, uint8_t *a_out)
{

}

void ccct_print_hex(uint8_t *a_buffer, size_t a_len)
{
    unsigned int i;
    unsigned int l_bytes_to_print = (100 / 48) * 16;
    for (i = 0; i < a_len; ++i) {
        if (i % l_bytes_to_print == 0)
            printf("\n");
        printf("%02X ", a_buffer[i]);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    printf("LZSS tester\n");
    const char *l_sample_seed = "The the and over";
    size_t l_sample_seed_size = strlen(l_sample_seed);
    printf("sample_seed_size: %ld\n", l_sample_seed_size);
    lzss_prepare_dictionary((uint8_t *)l_sample_seed, l_sample_seed_size);
    const char *l_plain = "The quick brown fox jumped over the lazy dog.";
    memcpy(plain + WINDOW_SIZE, l_plain, strlen(l_plain));
    lzss_encode(plain, strlen(l_plain), comp);
    printf("plain buffer contents with dictionary and plaintext:");
    ccct_print_hex(plain + (WINDOW_SIZE - l_sample_seed_size), l_sample_seed_size + strlen(l_plain));
    return 0;
}
