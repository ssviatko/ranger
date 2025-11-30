#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <limits.h>

#include "carith.h"

size_t g_segsize = 524288;
carith_comp_ctx ctx;

void process()
{
	carith_error_t err;

	err = carith_compress(&ctx);
	if (err != 0) printf(" error ");
	printf("-------compress: plain: %ld comp %ld freq %d rle_intermediate %ld total %ld\n", ctx.plain_len, ctx.comp_len, ctx.freq_comp_len, ctx.rle_intermediate, (ctx.comp_len + ctx.freq_comp_len));
	err = carith_extract(&ctx);
	if (err != 0) printf(" error ");
	printf("-------extract: decomp %ld rle_intermediate %ld ---------------------------------------- memcmp %d\n", ctx.decomp_len, ctx.rle_intermediate, memcmp(ctx.plain, ctx.decomp, ctx.plain_len));
	memset(ctx.plain, 0, g_segsize);
	memset(ctx.rleenc, 0, g_segsize);
	memset(ctx.comp, 0, g_segsize);
	memset(ctx.rledec, 0, g_segsize);
	memset(ctx.decomp, 0, g_segsize);
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
		res = read(lf_fd, ctx.plain, g_segsize);
		if (res < 0) {
			fprintf(stderr, "error: read %s: %s\n", a_path, strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (res == 0) continue;
		printf("******************** %s: %d bytes read\n", a_path, res);
		ctx.plain_len = res;
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
//				printf("directory\n");
				//listdir(entry->d_name);
			}
			break;
			case S_IFIFO:  printf("FIFO/pipe\n");               break;
			case S_IFLNK:  printf("symlink\n");                 break;
			case S_IFREG:
			{
//				printf("regular file\n");
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

	carith_init_ctx(&ctx, g_segsize);
	ctx.scheme = atoi(argv[2]);
	printf("scheme %02X\n", ctx.scheme);
	chdir(argv[1]);
	listdir(".");
	gettimeofday(&g_end_time, NULL);

	printf("completed operation in %ld seconds %ld usecs.\n",
		   g_end_time.tv_sec - g_start_time.tv_sec - ((g_end_time.tv_usec - g_start_time.tv_usec < 0) ? 1 : 0), // subtract 1 if there was a usec rollover
		g_end_time.tv_usec - g_start_time.tv_usec + ((g_end_time.tv_usec - g_start_time.tv_usec < 0) ? 1000000 : 0)); // bump usecs by 1 million usec for rollover

	return 0;
}
