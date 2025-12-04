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

#include "lzss.h"

#define WINDOW_SIZE 4095
#define SEGSIZE 524288

static uint8_t plain[WINDOW_SIZE + SEGSIZE];
static uint32_t plain_len;
static uint8_t comp[SEGSIZE * 3 / 2]; // 150% guard size
static uint8_t decomp[WINDOW_SIZE + (SEGSIZE * 3 /2)];
lzss_comp_ctx ctx;

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

void process()
{
     lzss_prepare_default_dictionary(&ctx, plain);
    lzss_prepare_pointer_pool(&ctx, plain, plain_len);
    printf("plain_len %d ", plain_len);
    size_t compsize;
    lzss_encode(&ctx, plain, plain_len, comp, &compsize);
    printf("comp (%ld bytes) ", compsize);
//    ccct_print_hex((uint8_t *)comp + WINDOW_SIZE, compsize);
    size_t decompsize;
    lzss_prepare_default_dictionary(&ctx, decomp);
    lzss_decode(&ctx, comp, compsize, decomp, &decompsize);
    printf("decomp (%ld bytes ratio %3.5f) ", decompsize, (float)compsize / (float)decompsize * 100.0);
//    ccct_print_hex((uint8_t *)decomp + WINDOW_SIZE, decompsize);
//    printf("P:%s\nD:%s\n", l_plain, decomp + WINDOW_SIZE);
    printf("memcmp plain/decomp: %d\n", memcmp(plain + WINDOW_SIZE, decomp + WINDOW_SIZE, decompsize));
//    for (size_t i = 0; i < decompsize; ++i) {
//        uint8_t pb = plain[WINDOW_SIZE + i];
//        uint8_t db = decomp[WINDOW_SIZE + i];
//        if (pb != db) {
//            printf("bytes at %ld differ: plain %02X decomp %02X\n", i, pb, db);
//        }
//    }
    return;
}

void load_file(const char *a_path)
{
    int lf_fd;
    int res;

    lf_fd = open(a_path, O_RDONLY);
    if (lf_fd < 0) {
        fprintf(stderr, "error: open %s: %s\n", a_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    do {
        res = read(lf_fd, plain + WINDOW_SIZE, SEGSIZE);
        if (res < 0) {
            fprintf(stderr, "error: read %s: %s\n", a_path, strerror(errno));
            exit(EXIT_FAILURE);
            process();
        }
        if (res == 0) continue;
        printf("%s: %d bytes read  ", a_path, res);
        plain_len = res;
        process();
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
                load_file(entry->d_name);
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

    lzss_error_t err = lzss_init_context(&ctx, SEGSIZE);
    if (err != LZSS_ERR_NONE) {
        fprintf(stderr, "init context");
        exit(EXIT_FAILURE);
    }

    chdir(argv[1]);
    listdir(".");
    //load_file("/home/ssviatko/c/pi.c");
    lzss_free_context(&ctx);

    gettimeofday(&g_end_time, NULL);

    printf("completed operation in %ld seconds %ld usecs.\n",
           g_end_time.tv_sec - g_start_time.tv_sec - ((g_end_time.tv_usec - g_start_time.tv_usec < 0) ? 1 : 0), // subtract 1 if there was a usec rollover
           g_end_time.tv_usec - g_start_time.tv_usec + ((g_end_time.tv_usec - g_start_time.tv_usec < 0) ? 1000000 : 0)); // bump usecs by 1 million usec for rollover

    return 0;
}
