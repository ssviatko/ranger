#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <limits.h>
#include <pthread.h>

#include "carith.h"

#pragma pack(1)

#define MAXTHREADS 48
#define DEFAULT_SEGSIZE 524288
#define BUFFLEN 1024 // general text buffer size

struct timeval g_start_time, g_end_time;
int g_debug = 0;
int g_verbose = 0;
int g_norle = 0;
int g_rleonly = 0;
uint32_t g_segsize = DEFAULT_SEGSIZE;
enum { MODE_NONE, MODE_COMPRESS, MODE_EXTRACT, MODE_TELL } g_mode = MODE_NONE;
char g_in[BUFFLEN];
int g_in_fd;
off_t g_in_len;
char g_out[BUFFLEN];
int g_out_fd;
const char *g_carith_suffix = ".carith";

uint16_t g_cookie = 0xd5aa;

// scheme bits - order of operations: RLE, then LZW, then AC.
const uint8_t scheme_ac = 0x80;
const uint8_t scheme_rle = 0x40;
const uint8_t scheme_lzw = 0x20;

typedef struct {
	uint16_t cookie; // network byte order
	uint8_t scheme;
	uint32_t plain_crc; // crc of plain input file
	uint32_t total_plain_len;
	uint32_t segsize;
} file_header;

/* Block format:
 *
 * uint24_t block comp_len (length on disk)
 * uint24_t block plain_len
 * frequency table
 * token table
 *
 * These are read/written manually and are not represented by a struct.
 */

// concurrency
int g_threads = 8; // default thread count
pthread_mutex_t g_tally_mtx;
pthread_cond_t g_tally_cond;
int g_tally = 0;
pthread_mutex_t g_debug_mtx; // protect debug messages in multithreaded environment

typedef struct {
	pthread_t thread;
	unsigned int id;
	int runflag;
	pthread_mutex_t sig_mtx;
	pthread_cond_t sig_cond;
	int sigflag;
	uint32_t cur_block;
} thread_work_area;

thread_work_area twa[MAXTHREADS];
carith_comp_ctx ctx[MAXTHREADS];

// options
enum {
	OPT_DEBUG = 1001,
	OPT_THREADS,
	OPT_NORLE,
	OPT_RLEONLY,
	OPT_NOCOLOR
};

struct option g_options[] = {
	{ "help", no_argument, NULL, '?' },
	{ "debug", no_argument, NULL, OPT_DEBUG },
	{ "verbose", no_argument, NULL, 'v' },
	{ "compress", no_argument, NULL, 'c' },
	{ "extract", no_argument, NULL, 'x' },
	{ "tell", no_argument, NULL, 't' },
	{ "threads", required_argument, NULL, OPT_THREADS },
	{ "segsize", required_argument, NULL, 'g' },
	{ "nocolor", no_argument, NULL, OPT_NOCOLOR },
	{ "norle", no_argument, NULL, OPT_NORLE },
	{ "rleonly", no_argument, NULL, OPT_RLEONLY },
	{ NULL, 0, NULL, 0 }
};

// color suport
int g_nocolor = 0;

#define CCCT_COLOR_HEADING   "\033[32m"          ///< Heading color
#define CCCT_COLOR_ERROR     "\033[91m"          ///< For error messages
#define CCCT_COLOR_HIGHLIGHT "\033[92m"          ///< Extra bright for highlights
#define CCCT_COLOR_BULLET    "\033[93m"          ///< Outstanding messages
#define CCCT_COLOR_DEFAULT   "\033[39m\033[49m"  ///< Return to default terminal color
#define CCCT_COLOR_DEBUG     "\033[33m"			 ///< Debug messages

void color_printf(const char *format, ...)
{
	char edited_format[BUFFLEN];
	char c;
	size_t i = 0, j = 0;
	enum { COLLECT, FINDSTAR } state = COLLECT;

	while (i < strlen(format)) {
		c = format[i];
		if (state == COLLECT) {
			if (c == '*') {
				// found first star
				state = FINDSTAR;
			} else {
				edited_format[j++] = c;
			}
			++i;
			continue;
		} else if (state == FINDSTAR) {
			if (c == '*') {
				// double star
				edited_format[j++] = '*';
			} else if (c == 'h') {
				// output highlight
				if (!g_nocolor) {
					edited_format[j] = 0;
					strcat(edited_format, CCCT_COLOR_HIGHLIGHT);
					j += strlen(CCCT_COLOR_HIGHLIGHT);
				}
			} else if (c == 'a') {
				if (!g_nocolor) {
					edited_format[j] = 0;
					strcat(edited_format, CCCT_COLOR_HEADING);
					j += strlen(CCCT_COLOR_HEADING);
				}
			} else if (c == 'b') {
				if (!g_nocolor) {
					edited_format[j] = 0;
					strcat(edited_format, CCCT_COLOR_BULLET);
					j += strlen(CCCT_COLOR_BULLET);
				}
			} else if (c == 'e') {
				if (!g_nocolor) {
					edited_format[j] = 0;
					strcat(edited_format, CCCT_COLOR_ERROR);
					j += strlen(CCCT_COLOR_ERROR);
				}
			} else if (c == 'd') {
				if (!g_nocolor) {
					edited_format[j] = 0;
					strcat(edited_format, CCCT_COLOR_DEFAULT);
					j += strlen(CCCT_COLOR_DEFAULT);
				}
			} else {
				// unknown escape sequence
				edited_format[j] = 0;
				strcat(edited_format, "*?");
				j += 2;
			}
			state = COLLECT;
			++i;
			continue;
		}
	}
	edited_format[j] = 0;

	va_list args;
	va_start(args, format);
	vprintf(edited_format, args);
	va_end(args);
}

void color_err_printf(int a_strerror, const char *format, ...)
{
	// call this without a linefeed at the end
	char edited_format[BUFFLEN];
	edited_format[0] = 0;
	if (!g_nocolor) strcat(edited_format, CCCT_COLOR_ERROR);
	strcat(edited_format, format);
	if (a_strerror) {
		sprintf(edited_format + strlen(edited_format), " : %s\n", strerror(errno));
	} else {
		sprintf(edited_format + strlen(edited_format), "\n");
	}
	if (!g_nocolor) strcat(edited_format, CCCT_COLOR_DEFAULT);
	va_list args;
	va_start(args, format);
	vfprintf(stderr, edited_format, args);
	va_end(args);
}

void color_debug(const char *format, ...)
{
	if (g_debug == 0)
		return; // don't print anything if debug isn't turned on
	pthread_mutex_lock(&g_debug_mtx);
	char edited_format[BUFFLEN];
	edited_format[0] = 0;
	if (!g_nocolor)
		strcat(edited_format, CCCT_COLOR_DEBUG);
	strcat(edited_format, format);
	if (!g_nocolor)
		strcat(edited_format, CCCT_COLOR_DEFAULT);
	va_list args;
	va_start(args, format);
	vprintf(edited_format, args);
	va_end(args);
	pthread_mutex_unlock(&g_debug_mtx);
}

void progress(uint32_t a_sofar, uint32_t a_total)
{
	static size_t l_lastsize = 0;
	int i;
	char l_txt[BUFFLEN];

	// cover over our previous message
	for (i = 0; i < l_lastsize; ++i)
		printf("\b");
	for (i = 0; i < l_lastsize; ++i)
		printf(" ");
	for (i = 0; i < l_lastsize; ++i)
		printf("\b");

	// print our message to l_txt to gauge the size on screen
	sprintf(l_txt, "(%u of %u) ", a_sofar, a_total);
	l_lastsize = strlen(l_txt);
	// now print it on screen in color with ansi escape codes
	if (g_verbose) color_printf("(*h%u*d of *h%u*d) ", a_sofar, a_total);
}

uint32_t g_crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t get_buffer_crc(uint32_t a_crcin, uint8_t *a_buff, size_t a_len)
{
	uint32_t l_crc = a_crcin;
	l_crc = l_crc ^ ~0U;

	size_t i;

	// compute CRC for res number of bytes
	for (i = 0; i < a_len; ++i) {
		l_crc = g_crc32_tab[(l_crc ^ a_buff[i]) & 0xFF] ^ (l_crc >> 8);
	}

	return l_crc ^ ~0U;
}

void verify_file_argument()
{
	// stat the user specified file to make sure it exists
	// no need to return anything as this will die if the file is not there or not a regular file
	int res;
	struct stat l_stat;
	res = stat(g_in, &l_stat);
	if (res < 0) {
		color_err_printf(1, "carith: unable to stat input file");
		exit(EXIT_FAILURE);
	}
	if ((l_stat.st_mode & S_IFMT) != S_IFREG) {
		color_err_printf(0, "carith: input file is not a regular file.");
		exit(EXIT_FAILURE);
	}
	if (l_stat.st_size > UINT_MAX) {
		color_err_printf(0, "carith: input file exceeds 4GB in size.");
		exit(EXIT_FAILURE);
	}
	g_in_len = l_stat.st_size;
	g_in_fd = open(g_in, O_RDONLY);
	if (g_in_fd < 0) {
		color_err_printf(1, "carith: can't open input file");
		exit(EXIT_FAILURE);
	}
	color_debug("opened input file %s, size %ld\n", g_in, g_in_len);
}

void *compress_tf(void *arg)
{
	thread_work_area *a_twa;
	a_twa = arg;

	while (1) {
		// wait to get signalled
		pthread_mutex_lock(&a_twa->sig_mtx);
		while ((a_twa->sigflag == 0) && (a_twa->runflag > 0)) {
			pthread_cond_wait(&a_twa->sig_cond, &a_twa->sig_mtx);
		}
		if (a_twa->runflag == 0) {
			// telling us to quit
			pthread_mutex_unlock(&a_twa->sig_mtx);
			// clean up
			pthread_exit(NULL);
		}
		pthread_mutex_unlock(&a_twa->sig_mtx);
		// signalled, so preform action
//		color_debug("tid %d got block %d to work on\n", a_twa->id, a_twa->cur_block);
		carith_compress(&ctx[a_twa->id]);
		color_debug("tid %d block %d plain_len %ld comp_len %ld freq_comp_len %ld total_comp_len %ld\n", a_twa->id, a_twa->cur_block, ctx[a_twa->id].plain_len, ctx[a_twa->id].comp_len,
					ctx[a_twa->id].freq_comp_len, (ctx[a_twa->id].comp_len + ctx[a_twa->id].freq_comp_len));
		// done
		a_twa->sigflag = 0;
		// signal doneness
		pthread_mutex_lock(&g_tally_mtx);
		g_tally++;
		pthread_cond_signal(&g_tally_cond);
		pthread_mutex_unlock(&g_tally_mtx);
	}
}

void compress()
{
	// compress file g_in
	int res;
	size_t i, j;
	int l_eof = 0;
	int l_docontinue = 0;
	uint32_t l_block_ctr = 0;
	file_header l_fh;

	// set output name
	g_out[0] = 0;
	strcpy(g_out, g_in);
	strcat(g_out, g_carith_suffix);
	color_debug("set output filename to %s\n", g_out);
	// open output file
	g_out_fd = open(g_out, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (g_out_fd < 0) {
		color_err_printf(1, "carith: unable to open output file");
		exit(EXIT_FAILURE);
	}
	// prepare file header + space in output file
	// we will rewind here later to populate the CRC and the total file size
	memset(&l_fh, 0, sizeof(l_fh));
	l_fh.cookie = htons(g_cookie);
	l_fh.scheme |= scheme_ac;
	l_fh.total_plain_len = htonl(g_in_len);
	l_fh.segsize = htonl(g_segsize);
	res = write(g_out_fd, &l_fh, sizeof(l_fh));
	if (res < 0) {
		color_err_printf(1, "carith: unable to write file header to output file.");
	}

	// spin up and init threads
	for (i = 0; i < g_threads; ++i) {
		pthread_mutex_init(&twa[i].sig_mtx, NULL);
		pthread_cond_init(&twa[i].sig_cond, NULL);
		twa[i].id = i;
		twa[i].runflag = 1;
		pthread_create(&twa[i].thread, NULL, compress_tf, &twa[i]);
	}

	uint32_t l_crc = 0;
	size_t l_sofar = 0;
	progress(l_sofar, g_in_len);

	do {
		g_tally = 0;
		// now read a bunch of g_segsize blocks
		for (i = 0; i < g_threads; ++i) {
			res = read(g_in_fd, ctx[i].plain, g_segsize);;
			if (res == 0) {
				color_debug("EOF on input file, bailing out\n");
				l_eof = 1;
				if (i == 0)
					l_docontinue = 1;
				break;
				// at this point, i contains the number of bytes successfully read
			} else if (res < 0) {
				color_err_printf(1, "carith: unable to read input file");
				exit(EXIT_FAILURE);
			}
			color_debug("read block %d from input file len %ld\n", l_block_ctr, res);
			// compute crc on input file here
			ctx[i].plain_len = res;
			l_sofar += res;
			l_crc = get_buffer_crc(l_crc, ctx[i].plain, ctx[i].plain_len);
			// populate a thread and signal it
			pthread_mutex_lock(&twa[i].sig_mtx);
			twa[i].cur_block = l_block_ctr;
			twa[i].sigflag = 1;
			pthread_cond_signal(&twa[i].sig_cond);
			pthread_mutex_unlock(&twa[i].sig_mtx);
			l_block_ctr++;
		}
		if (l_docontinue > 0)
			continue; // go down to bottom of do loop

		color_debug("waiting for threads to finish\n");
		// wait for threads to finish
		pthread_mutex_lock(&g_tally_mtx);
		while (g_tally < i)
			pthread_cond_wait(&g_tally_cond, &g_tally_mtx);
		pthread_mutex_unlock(&g_tally_mtx);
		// all our threads are done and the plains are all contained in the ctx data structures
		color_debug("processing %d blocks\n", i);
		for (j = 0; j < i; ++j) {
			// write comp_len + freq_comp_len to file as network byte order uint32_t
			uint32_t l_total = ctx[j].comp_len + ctx[j].freq_comp_len;
			l_total = htonl(l_total);
			res = write(g_out_fd, &l_total, sizeof(l_total));
			if (res < 0) {
				color_err_printf(1, "carith: unable to write to output file.");
				exit(EXIT_FAILURE);
			}
			if (res != sizeof(l_total)) {
				color_err_printf(0, "carith: difficulty writing to output file: wrote %ld expected to write %ld.", res, sizeof(l_total));
				exit(EXIT_FAILURE);
			}
			// write frequency table
			res = write(g_out_fd, ctx[j].freq_comp, ctx[j].freq_comp_len);
			if (res < 0) {
				color_err_printf(1, "carith: unable to write to output file.");
				exit(EXIT_FAILURE);
			}
			if (res != ctx[j].freq_comp_len) {
				color_err_printf(0, "carith: difficulty writing to output file: wrote %ld expected to write %ld.", res, ctx[j].freq_comp_len);
				exit(EXIT_FAILURE);
			}
			// write token table
			res = write(g_out_fd, ctx[j].comp, ctx[j].comp_len);
			if (res < 0) {
				color_err_printf(1, "carith: unable to write to output file.");
				exit(EXIT_FAILURE);
			}
			if (res != ctx[j].comp_len) {
				color_err_printf(0, "carith: difficulty writing to output file: wrote %ld expected to write %ld.", res, ctx[j].comp_len);
				exit(EXIT_FAILURE);
			}
		}
		progress(l_sofar, g_in_len);
	} while (l_eof == 0);
	if (g_verbose) printf("\n"); // after progress meter

	color_debug("input file CRC: %08X\n", l_crc);
	l_fh.plain_crc = htonl(l_crc);
	res = lseek(g_out_fd, 0, SEEK_SET);
	if (res < 0) {
		color_err_printf(1, "carith: unable to seek output file.");
		exit(EXIT_FAILURE);
	}
	res = write(g_out_fd, &l_fh, sizeof(l_fh));
	if (res < 0) {
		color_err_printf(1, "carith: unable to write file header to output file after compression.");
	}
	// how big is the output file in totality?
	struct stat l_stat;
	res = stat(g_out, &l_stat);
	if (res < 0) {
		color_err_printf(1, "carith: unable to stat output file");
		exit(EXIT_FAILURE);
	}
	if (g_verbose) color_printf("*acarith:*d compressed *h%s*d into *h%s*d (ratio *h%3.5f%%*d)\n", g_in, g_out, (float)(l_stat.st_size) / (float)(ntohl(l_fh.total_plain_len)) * 100.0);

	color_debug("joining threads...\n");
	// join threads
	for (i = 0; i < g_threads; ++i) {
		pthread_mutex_lock(&twa[i].sig_mtx);
		twa[i].runflag = 0;
		pthread_cond_signal(&twa[i].sig_cond);
		pthread_mutex_unlock(&twa[i].sig_mtx);
		pthread_join(twa[i].thread, NULL);
	}
	color_debug("destroying synchronization primitives...\n");
	// clean up
	for (i = 0; i < g_threads; ++i) {
		pthread_mutex_destroy(&twa[i].sig_mtx);
		pthread_cond_destroy(&twa[i].sig_cond);
	}

	color_debug("closing files and exiting\n");
	close(g_in_fd);
	close(g_out_fd);
}

void extract()
{
	// extract file g_in
}

void tell()
{
	// info about g_in
}

int main(int argc, char **argv)
{
	int opt;
	size_t i;

	// try to determine hardware concurrency
	unsigned int l_tcnt = sysconf(_SC_NPROCESSORS_ONLN);
	if (l_tcnt != 0) {
		g_threads = l_tcnt;
	}

	// initialize threaded environment
	pthread_mutex_init(&g_debug_mtx, NULL);
	pthread_mutex_init(&g_tally_mtx, NULL);
	pthread_cond_init(&g_tally_cond, NULL);


	while ((opt = getopt_long(argc, argv, "?g:vcxt", g_options, NULL)) != -1) {
		switch (opt) {
			case OPT_DEBUG:
			{
				g_debug = 1;
			}
			break;
			case OPT_NOCOLOR:
			{
				g_nocolor = 1;
			}
			break;
			case OPT_THREADS: // force thread count
			{
				g_threads = atoi(optarg);
			}
			break;
			case 'g': // segsize
			{
				g_segsize = atoi(optarg);
			}
			break;
			case 'v': // verbose
			{
				g_verbose = 1;
			}
			break;
			case 'c': // compress
			{
				if (g_mode != MODE_NONE) {
					color_err_printf(0, "carith: please select only one operational mode.");
					exit(EXIT_FAILURE);
				}
				g_mode = MODE_COMPRESS;
			}
			break;
			case 'x': // extract
			{
				if (g_mode != MODE_NONE) {
					color_err_printf(0, "carith: please select only one operational mode.");
					exit(EXIT_FAILURE);
				}
				g_mode = MODE_EXTRACT;
			}
			break;
			case 't': // tell
			{
				if (g_mode != MODE_NONE) {
					color_err_printf(0, "carith: please select only one operational mode.");
					exit(EXIT_FAILURE);
				}
				g_mode = MODE_TELL;
			}
			break;
			case OPT_NORLE:
			{
				g_norle = 1;
			}
			break;
			case OPT_RLEONLY:
			{
				g_rleonly = 1;
			}
			break;
			case '?':
			{
				color_printf("*hcarith (C arithmetic coder) compression utility*d\n");
				color_printf("build *h%s*d release *h%s*d built on *h%s*d\n", BUILD_NUMBER, RELEASE_NUMBER, BUILD_DATE);
				color_printf("*aby Stephen Sviatko - (C) 2025 Good Neighbors LLC*d\n");
				color_printf("*husage:*a carith <options> <file>*d\n");
				color_printf("*a  -? (--help)*d this screen\n");
				color_printf("*a     (--debug)*d enable debug mode\n");
				color_printf("*a     (--nocolor)*d defeat colors\n");
				color_printf("*a     (--threads) <count>*d specify number of theads to use (default *h%d*d)\n", g_threads);
				color_printf("*a  -g (--segsize) <bytes>*d specify size of segments (default *h%d*d)\n", DEFAULT_SEGSIZE);
				color_printf("*a  -v (--verbose)*d enable verbose mode\n");
				color_printf("*a     (--norle)*d defeat RLE encode before arithmetic compression\n");
				color_printf("*a     (--rleonly)*d RLE encode file only, no arithmetic compression\n");
				color_printf("*hoperational modes*a (choose only one)*d\n");
				color_printf("*a  -c (--compress) <file>*d compress a file\n");
				color_printf("*a  -x (--extract) <file.carith>*d extract a file\n");
				color_printf("*a  -t (--tell) <file.carith>*d show contents of compressed file\n");
				exit(EXIT_SUCCESS);
			}
			break;
		}
	}

	setbuf(stdout, NULL); // disable buffering so we can print our ccct_progress

	// police illogical RLE choices
	if ((g_norle == 1) && (g_rleonly == 1)) {
		color_err_printf(0, "carith: --norle and --rleonly are mutually exclusive. please select only one of these.");
		color_err_printf(0, "carith: use -? or --help for usage information.");
		exit(EXIT_FAILURE);
	}

	// police thread count
	if (g_threads < 1) {
		color_err_printf(0, "carith: need to use at least 1 thread.");
		exit(EXIT_FAILURE);
	}
	if (g_threads > MAXTHREADS) {
		color_err_printf(0, "carith: thread limit: %d.", MAXTHREADS);
		exit(EXIT_FAILURE);
	}
	if (g_threads > 1) {
		if (g_verbose) color_printf("*acarith:*d enabling *h%d*d threads.\n", g_threads);
	}

	// police segsize
	if (g_segsize < 32768) {
		color_err_printf(0, "carith: need to use segment size of oat least 32768.");
		exit(EXIT_FAILURE);
	}
	if (g_segsize > 16777216) {
		color_err_printf(0, "carith: segment size limit: 16777216.");
		exit(EXIT_FAILURE);
	}
	if (g_verbose) color_printf("*acarith:*d using *h%d*d byte segment size.\n", g_segsize);

	gettimeofday(&g_start_time, NULL);

	// init carith contexts
	carith_error_t init_error;
	for (i = 0; i < g_threads; ++i) {
		init_error = carith_init_ctx(&ctx[i], g_segsize);
		if (init_error != CARITH_ERR_NONE) {
			color_err_printf(0, "carith_init_ctx retuned %s.\n", carith_strerror(init_error));
			exit(EXIT_FAILURE);
		}
	}

	if (g_mode == MODE_COMPRESS) {
		if (optind >= argc) {
			color_err_printf(0, "carith: expected file argument.");
			exit(EXIT_FAILURE);
		}
		if (g_verbose) color_printf("*acarith:*d compressing *h%s*d ... ", argv[optind]);
		if (g_verbose && g_norle) color_printf("*acarith:*d defeating RLE encode before arithmetic compression.\n");
		if (g_verbose && g_rleonly) color_printf("*acarith:*d RLE encode file only, no arithmetic compression.\n");
		g_in[0] = 0;
		strcpy(g_in, argv[optind]);
		verify_file_argument();
		compress();
	} else if (g_mode == MODE_EXTRACT) {
		if (optind >= argc) {
			color_err_printf(0, "carith: expected file argument.");
			exit(EXIT_FAILURE);
		}
		if (g_verbose) color_printf("*acarith:*d extracting *h%s*d\n", argv[optind]);
		g_in[0] = 0;
		strcpy(g_in, argv[optind]);
		verify_file_argument();
		extract();
	} else if (g_mode == MODE_TELL) {
		if (optind >= argc) {
			color_err_printf(0, "carith: expected file argument.");
			exit(EXIT_FAILURE);
		}
		if (g_verbose) color_printf("*acarith:*d telling contents of *h%s*d\n", argv[optind]);
		g_in[0] = 0;
		strcpy(g_in, argv[optind]);
		verify_file_argument();
		tell();
	} else {
		color_err_printf(0, "carith: please choose at least one operational mode.");
		color_err_printf(0, "carith: use -? or --help for usage information.");
		exit(EXIT_FAILURE);
	}

	// free
	for (i = 0; i < g_threads; ++i) {
		carith_free_ctx(&ctx[i]);
	}
	pthread_cond_destroy(&g_tally_cond);
	pthread_mutex_destroy(&g_tally_mtx);
	pthread_mutex_destroy(&g_debug_mtx);

	gettimeofday(&g_end_time, NULL);
	if (g_verbose) color_printf("*acarith:*d completed operation in *h%ld*d seconds *h%ld*d usecs.\n",
		g_end_time.tv_sec - g_start_time.tv_sec - ((g_end_time.tv_usec - g_start_time.tv_usec < 0) ? 1 : 0), // subtract 1 if there was a usec rollover
		g_end_time.tv_usec - g_start_time.tv_usec + ((g_end_time.tv_usec - g_start_time.tv_usec < 0) ? 1000000 : 0)); // bump usecs by 1 million usec for rollover

	return 0;
}

