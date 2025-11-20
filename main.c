#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/fcntl.h>
#include <unistd.h>

#define WORKBITS 18
#define WORKSIZE 262144 ///< 2^BITS

uint8_t work[WORKSIZE];
size_t work_len;

typedef struct {
	uint64_t count;
	uint64_t range_start;
	uint64_t range_end;
} freq_entry;

freq_entry freq_tbl[256];

void freq_count()
{
	size_t i;
	for (i = 0; i < 256; ++i) {
		freq_tbl[i].count = 0;
	}
	for (i = 0; i < work_len; ++i) {
		freq_tbl[work[i]].count++;
	}
}

void assign_ranges(uint64_t a_start, uint64_t a_end)
{
	size_t i;
	uint64_t l_rangesize = a_end - a_start;
	uint64_t l_countbase = 0;
	uint64_t l_rangebase = 0;

	for (i = 0; i < 256; ++i) {
		freq_tbl[i].range_start = a_start + l_rangebase;
		l_countbase += freq_tbl[i].count;
		l_rangebase = (l_countbase * l_rangesize) / work_len;
		freq_tbl[i].range_end = a_start + l_rangebase;
	}
}

void process()
{
	size_t i, j, l = 0;

	if (work_len == 0)
		return;

	freq_count();
	assign_ranges(0, 0x10000000000);
	for (j = 0; j < 32; ++j) {
		for (i = 0; i < 8; ++i) {
			size_t k = (j * 8) + i;
			if (freq_tbl[k].count > 0) {
				printf("%02lX: %ld %010lX %010lX  ", k, freq_tbl[k].count, freq_tbl[k].range_start, freq_tbl[k].range_end);
				l++;
				if (l == 5) {
					l = 0;
					printf("\n");
				}
			}
		}
	}
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
	res = read(lf_fd, work, WORKSIZE);
	if (res < 0) {
		fprintf(stderr, "error: read %s: %s\n", a_path, strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("%s: %d bytes read\n", a_path, res);
	work_len = res;
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
		printf("%s: ", entry->d_name);
		struct stat l_stat;
		res = stat(entry->d_name, &l_stat);
		if (res < 0) {
			fprintf(stderr, "error: stat: %s\n", strerror(errno));
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
				printf("regular file\n");
				load_file(entry->d_name);
				process();
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
	printf("cranger build %s release %s\nbuilt on %s\n", BUILD_NUMBER, RELEASE_NUMBER, BUILD_DATE);
	listdir(".");
	return 0;
}
