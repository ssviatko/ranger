#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h> // for htons/htonl
#include <sys/fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>

#include "lzss4.h"
#include "lzss32.h"

#define LZSS4_WINDOW_SIZE 4095
#define LZSS32_WINDOW_SIZE 32767
#define SEGSIZE 524288

static uint8_t plain4[LZSS4_WINDOW_SIZE + SEGSIZE];
static uint32_t plain4_len;
static uint8_t comp[SEGSIZE * 3 / 2]; // 150% guard size
static uint8_t decomp4[LZSS4_WINDOW_SIZE + (SEGSIZE * 3 /2)];
lzss4_comp_ctx ctx4;

static uint8_t plain32[LZSS32_WINDOW_SIZE + SEGSIZE];
static uint32_t plain32_len;
static uint8_t decomp32[LZSS32_WINDOW_SIZE + (SEGSIZE * 3 /2)];
lzss32_comp_ctx ctx32;

void ccct_print_hex(uint8_t *a_buffer, size_t a_len)
{
    unsigned int i;
    unsigned int l_bytes_to_print = (200 / 48) * 16;
    for (i = 0; i < a_len; ++i) {
        if (i % l_bytes_to_print == 0)
            printf("\n");
        printf("%02X ", a_buffer[i]);
    }
    printf("\n");
}

void process4()
{
    lzss4_prepare_default_dictionary(&ctx4, plain4);
    lzss4_prepare_pointer_pool(&ctx4, plain4, plain4_len);
    printf("plain_len4 %d ", plain4_len);
    //    ccct_print_hex((uint8_t *)plain + WINDOW_SIZE, plain_len);
    //    printf("dictionary: start at %d ", ctx.seed_dictionary_start);
    //    ccct_print_hex(plain + ctx.seed_dictionary_start, WINDOW_SIZE - ctx.seed_dictionary_start);
    size_t compsize;
    lzss4_encode(&ctx4, plain4, plain4_len, comp, &compsize);
    printf("comp (%ld bytes) ", compsize);
    //    ccct_print_hex((uint8_t *)comp, compsize);
    size_t decompsize4;
    lzss4_prepare_default_dictionary(&ctx4, decomp4);
    lzss4_decode(&ctx4, comp, compsize, decomp4, &decompsize4);
    printf("decomp4 (%ld bytes ratio %3.5f) ", decompsize4, (float)compsize / (float)decompsize4 * 100.0);
    //    ccct_print_hex((uint8_t *)decomp + WINDOW_SIZE, decompsize);
    //    printf("P:%s\nD:%s\n", l_plain, decomp + WINDOW_SIZE);
    printf("memcmp plain4/decomp4: %d\n", memcmp(plain4 + LZSS4_WINDOW_SIZE, decomp4 + LZSS4_WINDOW_SIZE, decompsize4));
    //    for (size_t i = 0; i < decompsize; ++i) {
    //        uint8_t pb = plain[WINDOW_SIZE + i];
    //        uint8_t db = decomp[WINDOW_SIZE + i];
    //        if (pb != db) {
    //            printf("bytes at %ld differ: plain %02X decomp %02X\n", i, pb, db);
    //        }
    //    }
    return;
}

void process32()
{
    lzss32_prepare_default_dictionary(&ctx32, plain32);
    lzss32_prepare_pointer_pool(&ctx32, plain32, plain32_len);
    printf("plain32_len %d ", plain32_len);
//    ccct_print_hex((uint8_t *)plain32 + LZSS32_WINDOW_SIZE, plain32_len);
    //    printf("dictionary: start at %d ", ctx.seed_dictionary_start);
    //    ccct_print_hex(plain + ctx.seed_dictionary_start, WINDOW_SIZE - ctx.seed_dictionary_start);
    size_t compsize;
    lzss32_encode(&ctx32, plain32, plain32_len, comp, &compsize);
    printf("comp (%ld bytes) ", compsize);
//    ccct_print_hex((uint8_t *)comp, compsize);
    size_t decompsize32;
    lzss32_prepare_default_dictionary(&ctx32, decomp32);
    lzss32_decode(&ctx32, comp, compsize, decomp32, &decompsize32);
    printf("decomp32 (%ld bytes ratio %3.5f) ", decompsize32, (float)compsize / (float)decompsize32 * 100.0);
//    ccct_print_hex((uint8_t *)decomp32 + LZSS32_WINDOW_SIZE, decompsize32);
    //    printf("P:%s\nD:%s\n", l_plain, decomp + WINDOW_SIZE);
    printf("memcmp plain32/decomp32: %d\n", memcmp(plain32 + LZSS32_WINDOW_SIZE, decomp32 + LZSS32_WINDOW_SIZE, decompsize32));
    //    for (size_t i = 0; i < decompsize; ++i) {
    //        uint8_t pb = plain[WINDOW_SIZE + i];
    //        uint8_t db = decomp[WINDOW_SIZE + i];
    //        if (pb != db) {
    //            printf("bytes at %ld differ: plain %02X decomp %02X\n", i, pb, db);
    //        }
    //    }
    return;
}

void load_file4(const char *a_path)
{
    int lf_fd;
    int res;

    lf_fd = open(a_path, O_RDONLY);
    if (lf_fd < 0) {
        fprintf(stderr, "error: open %s: %s\n", a_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    do {
        res = read(lf_fd, plain4 + LZSS4_WINDOW_SIZE, SEGSIZE);
        if (res < 0) {
            fprintf(stderr, "error: read %s: %s\n", a_path, strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (res == 0) continue;
        printf("%s: %d bytes read  ", a_path, res);
        plain4_len = res;
        process4();
    } while (res != 0);
    close(lf_fd);
}

void load_file32(const char *a_path)
{
    int lf_fd;
    int res;

    lf_fd = open(a_path, O_RDONLY);
    if (lf_fd < 0) {
        fprintf(stderr, "error: open %s: %s\n", a_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    do {
        res = read(lf_fd, plain32 + LZSS32_WINDOW_SIZE, SEGSIZE);
        if (res < 0) {
            fprintf(stderr, "error: read %s: %s\n", a_path, strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (res == 0) continue;
        printf("%s: %d bytes read  ", a_path, res);
        plain32_len = res;
        process32();
    } while (res != 0);
    close(lf_fd);
}

int listdir(const char *path) {
    struct dirent *entry;
    DIR *dp;
    int res;

    dp = opendir(path);
    if (dp == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dp)) != NULL) {
        //		printf("%s: ", entry->d_name);
        struct stat l_stat;
        res = stat(entry->d_name, &l_stat);
        if (res < 0) {
            fprintf(stderr, "error: stat on %s: %s\n", entry->d_name, strerror(errno));
            exit(EXIT_FAILURE);
        }
        switch (l_stat.st_mode & S_IFMT) {
            case S_IFBLK:  printf("block device\n");            break;
            case S_IFCHR:  printf("character device\n");        break;
            case S_IFDIR:
            {
                printf("directory\n");
                //listdir(entry->d_name);
            }
            break;
            case S_IFIFO:  printf("FIFO/pipe\n");               break;
            case S_IFLNK:  printf("symlink\n");                 break;
            case S_IFREG:
            {
                //printf("regular file\n");
                load_file32(entry->d_name);
                load_file4(entry->d_name);
            }
            break;
            case S_IFSOCK: printf("socket\n");                  break;
            default:       printf("unknown?\n");                break;
        }
    }

    closedir(dp);
    return 0;
}

int main(int argc, char **argv)
{
    struct timeval g_start_time, g_end_time;
    gettimeofday(&g_start_time, NULL);

    lzss4_error_t err4 = lzss4_init_context(&ctx4, SEGSIZE);
    if (err4 != LZSS_ERR_NONE) {
        fprintf(stderr, "init context");
        exit(EXIT_FAILURE);
    }
    lzss32_error_t err32 = lzss32_init_context(&ctx32, SEGSIZE);
    if (err32 != LZSS32_ERR_NONE) {
        fprintf(stderr, "init context");
        exit(EXIT_FAILURE);
    }

    chdir(argv[1]);
    listdir(".");
//    load_file32(argv[1]);
//    load_file4(argv[1]);
    lzss4_free_context(&ctx4);
    lzss32_free_context(&ctx32);

    gettimeofday(&g_end_time, NULL);

    printf("completed operation in %ld seconds %ld usecs.\n",
           g_end_time.tv_sec - g_start_time.tv_sec - ((g_end_time.tv_usec - g_start_time.tv_usec < 0) ? 1 : 0), // subtract 1 if there was a usec rollover
           g_end_time.tv_usec - g_start_time.tv_usec + ((g_end_time.tv_usec - g_start_time.tv_usec < 0) ? 1000000 : 0)); // bump usecs by 1 million usec for rollover

    return 0;
}
