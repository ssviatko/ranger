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
#include <sys/types.h>
#include <arpa/inet.h>
#include <limits.h>
#include <pthread.h>

#include "carith.h"
#include "color_print.h"
#include "crc32.h"

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
mode_t g_in_mode;
char g_out[BUFFLEN];
int g_out_fd;
const char *g_carith_suffix = ".carith";
int g_keep = 1;
const char *g_keep_suffix = ".plain";
char g_dmbuff[4];

uint16_t g_cookie = 0xd5aa;

typedef struct {
	uint16_t cookie; // network byte order
	uint8_t scheme;
	mode_t mode; // mode of original file
	uint32_t plain_crc; // crc of plain input file
	uint32_t total_plain_len;
	uint32_t total_rle_len;
	uint32_t segsize;
} file_header;

// concurrency
int g_threads = 8; // default thread count
pthread_mutex_t g_tally_mtx;
pthread_cond_t g_tally_cond;
int g_tally = 0;

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
	OPT_NOCOLOR,
	OPT_NOKEEP
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
	{ "nokeep", no_argument, NULL, OPT_NOKEEP },
	{ NULL, 0, NULL, 0 }
};

// color suport
int g_nocolor = 0;

char *decimal_mode(mode_t a_mode)
{
	char u, g, o;

	u = ((a_mode >> 6) & 7) | 0x30;
	g = ((a_mode >> 3) & 7) | 0x30;
	o = (a_mode & 7) | 0x30;
	g_dmbuff[0] = u;
	g_dmbuff[1] = g;
	g_dmbuff[2] = o;
	g_dmbuff[3] = 0;
	return g_dmbuff;
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
	g_in_mode = l_stat.st_mode;
	color_debug("g_in stat - st_size %ld st_mode %08lX (%s)\n", l_stat.st_size, l_stat.st_mode, decimal_mode(l_stat.st_mode));
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
		if (g_mode == MODE_COMPRESS) {
			carith_compress(&ctx[a_twa->id]);
			color_debug("tid %d block %d plain_len %ld comp_len %ld freq_comp_len %ld total_comp_len %ld plainCRC %08X compCRC %08X\n", a_twa->id, a_twa->cur_block, ctx[a_twa->id].plain_len, ctx[a_twa->id].comp_len, ctx[a_twa->id].freq_comp_len, (ctx[a_twa->id].comp_len + ctx[a_twa->id].freq_comp_len), get_buffer_crc(0, ctx[a_twa->id].plain, ctx[a_twa->id].plain_len), get_buffer_crc(0, ctx[a_twa->id].comp, ctx[a_twa->id].comp_len));
		} else if (g_mode == MODE_EXTRACT) {
			carith_extract(&ctx[a_twa->id]);
			color_debug("tid %d block %d decomp_len %ld total_comp_len %ld compCRC %08X decompCRC %08X\n", a_twa->id, a_twa->cur_block, ctx[a_twa->id].decomp_len, (ctx[a_twa->id].comp_len + ctx[a_twa->id].freq_comp_len), get_buffer_crc(0, ctx[a_twa->id].comp, ctx[a_twa->id].comp_len), get_buffer_crc(0, ctx[a_twa->id].decomp, ctx[a_twa->id].decomp_len));
		}
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
	size_t l_sofar;

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
	l_fh.mode = htonl(g_in_mode);
	if (g_rleonly) {
		l_fh.scheme |= scheme_rle;
	} else if (g_norle) {
		l_fh.scheme |= scheme_ac;
	} else {
		l_fh.scheme |= scheme_ac;
		l_fh.scheme |= scheme_rle;
	}
	l_fh.total_plain_len = htonl(g_in_len);
	l_fh.segsize = htonl(g_segsize);
	l_fh.total_rle_len = 0;
	res = write(g_out_fd, &l_fh, sizeof(l_fh));
	if (res < 0) {
		color_err_printf(1, "carith: unable to write file header to output file.");
	}

	if (g_verbose) color_printf("*acarith:*d compressing *h%s*d ... ", g_in);

	// spin up and init threads
	for (i = 0; i < g_threads; ++i) {
		pthread_mutex_init(&twa[i].sig_mtx, NULL);
		pthread_cond_init(&twa[i].sig_cond, NULL);
		twa[i].id = i;
		twa[i].runflag = 1;
		pthread_create(&twa[i].thread, NULL, compress_tf, &twa[i]);
	}

	uint32_t l_crc = 0;
	uint32_t l_block_crc = 0;
	l_sofar = 0;
	if (g_verbose) color_progress(l_sofar, g_in_len);

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
			// compute crc on input file here
			ctx[i].plain_len = res;
			ctx[i].scheme = l_fh.scheme;
			l_sofar += res;
			l_crc = get_buffer_crc(l_crc, ctx[i].plain, ctx[i].plain_len);
			l_block_crc = get_buffer_crc(0, ctx[i].plain, ctx[i].plain_len);
			color_debug("read block %d from input file len %ld block CRC %08X\n", l_block_ctr, res, l_block_crc);
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
			// write rle_intermediate to file as network byte order uint32_t
			uint32_t l_total = ctx[j].rle_intermediate;
			color_debug("block %ld writing rle_intermediate %ld\n", j, l_total);
			l_fh.total_rle_len += l_total;
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
			// write comp_len + freq_comp_len to file as network byte order uint32_t
			l_total = ctx[j].comp_len + ctx[j].freq_comp_len;
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
			// write freq_comp_len to file as network byte order uint16_t
			uint16_t l_freqtotal = ctx[j].freq_comp_len;
			l_freqtotal = htons(l_freqtotal);
			res = write(g_out_fd, &l_freqtotal, sizeof(l_freqtotal));
			if (res < 0) {
				color_err_printf(1, "carith: unable to write to output file.");
				exit(EXIT_FAILURE);
			}
			if (res != sizeof(l_freqtotal)) {
				color_err_printf(0, "carith: difficulty writing to output file: wrote %ld expected to write %ld.", res, sizeof(l_freqtotal));
				exit(EXIT_FAILURE);
			}
			// write block plain_len so the decompressor will know when to stop
			l_total = htonl(ctx[j].plain_len);
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
		if (g_verbose) color_progress(l_sofar, g_in_len);
	} while (l_eof == 0);
	if (g_verbose) printf("\n"); // after color_progress meter

	color_debug("input file CRC: %08X\n", l_crc);
	l_fh.plain_crc = htonl(l_crc);

	// user warnings
	int l_warn_norle = 0;
	int l_warn_rleonly = 0;
	int l_warn_aconly = 0;
	// check total_rle_len, is it sane?
	if (l_fh.total_rle_len > g_in_len)
		l_warn_norle = 1;

	l_fh.total_rle_len = htonl(l_fh.total_rle_len);

	// seek output back and write out updated file header
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

	// did AC encoding with RLE increase the size of the file?
	size_t l_complen = l_stat.st_size - sizeof(l_fh); // all the crap past the file header
	if (((l_fh.scheme & 0xc0) == 0xc0) && (l_complen > ntohl(l_fh.total_rle_len)))
		l_warn_rleonly = 1;
	// did AC encoding by itself increase the size of the file?
	if (((l_fh.scheme & 0xc0) == scheme_ac) && (l_stat.st_size > g_in_len))
		l_warn_aconly = 1;

	if (g_verbose) {
		if (l_warn_norle && l_warn_rleonly) {
			// can't advise the user to use both switches!
			color_printf("*acarith:*d *ewarning:*d both RLE encoding and arithmetic coding caused file size to increase.\n*acarith:*d file *h%s*d can not be compressed efficiently with *acarith*d.\n", g_in);
		} else {
			if (l_warn_norle) {
				color_printf("*acarith:*d *ewarning:*d RLE encoding caused file size to bloom from *h%ld*d to *h%ld*d.\n*acarith:*d use *h--norle*d switch to get better compression ratio.\n", g_in_len, ntohl(l_fh.total_rle_len));
			}
			if (l_warn_rleonly) { // warn user that ditching AC will mean a smaller file
				color_printf("*acarith:*d *ewarning:*d arithmetic compression caused RLE output to bloom from *h%ld*d to final size of *h%ld*d.\n*acarith:*d use *h--rleonly*d switch to get better compression ratio.\n", ntohl(l_fh.total_rle_len), l_complen);
			}
			if (l_warn_aconly) { // warn user that AC by itself isn't cutting it
				color_printf("*acarith:*d *ewarning:*d arithmetic compression by itself caused file size to bloom from *h%ld*d to *h%ld*d.\n*acarith:*d use RLE to possibly get better compression ratio.\n", g_in_len, l_stat.st_size);
			}
		}
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

	if (g_keep == 0) {
		// unlink g_in
		unlink(g_in);
	}

	color_debug("closing files and exiting\n");
	close(g_in_fd);
	close(g_out_fd);
}

void extract()
{
	size_t i, j;
	int res;
	file_header l_fh;
	struct stat l_in_stat;

	res = read(g_in_fd, &l_fh, sizeof(l_fh));
	if (res < 0) {
		color_err_printf(1, "unable to read input file");
		exit(EXIT_FAILURE);
	}
	res = stat(g_in, &l_in_stat);
	if (res < 0) {
		color_err_printf(1, "unable to stat input file");
		exit(EXIT_FAILURE);
	}

	// tell mode is always verbose
	if (g_mode == MODE_TELL)
		g_verbose = 1;

	if (ntohs(l_fh.cookie) == g_cookie) {
		if (g_verbose) {
			color_printf("*acarith:*d --- original file length: *h%ld*d\n", ntohl(l_fh.total_plain_len));
			color_printf("*acarith:*d --- original file mode:   *h%08lX*d (*h%s*d)\n", ntohl(l_fh.mode), decimal_mode(ntohl(l_fh.mode)));
			color_printf("*acarith:*d --- compression ratio:    *h%3.5f*d\n", (float)l_in_stat.st_size / (float)ntohl(l_fh.total_plain_len) * 100.0);
			color_printf("*acarith:*d --- block size:           *h%d*d\n", ntohl(l_fh.segsize));
			color_printf("*acarith:*d --- original file CRC:    *h%08X*d\n", ntohl(l_fh.plain_crc));
			color_printf("*acarith:*d --- compression chain:    ");
			if ((l_fh.scheme & scheme_rle) == scheme_rle)
				color_printf("*hRLE *d");
			if ((l_fh.scheme & scheme_lzw) == scheme_lzw)
				color_printf("*hLZW *d");
			if ((l_fh.scheme & scheme_ac) == scheme_ac)
				color_printf("*hAC *d");
			printf("\n");
			if ((l_fh.scheme & scheme_rle) == scheme_rle) {
				color_printf("*acarith:*d --- RLE intermediate:     *h%ld*d\n", ntohl(l_fh.total_rle_len));
			}
		}
	} else {
		color_err_printf(0, "carith: file is not a carith archive.");
		close(g_in_fd);
		exit(EXIT_FAILURE);
	}

	if (g_mode == MODE_TELL) {
		close(g_in_fd);
		return;
	}

	// if block size differs from what we have selected with -g (or our default), then recycle it
	if (g_segsize != ntohl(l_fh.segsize)) {
		color_debug("changing segsize from %d to %d\n", g_segsize, ntohl(l_fh.segsize));
		carith_error_t init_error;
		g_segsize = ntohl(l_fh.segsize);
		// dispose them all
		for (i = 0; i < g_threads; ++i) {
			carith_free_ctx(&ctx[i]);
		}
		// then init them again
		for (i = 0; i < g_threads; ++i) {
			init_error = carith_init_ctx(&ctx[i], g_segsize);
			if (init_error != CARITH_ERR_NONE) {
				color_err_printf(0, "carith_init_ctx retuned %s.\n", carith_strerror(init_error));
				exit(EXIT_FAILURE);
			}
		}
	}

	// create output file name
	strcpy(g_out, g_in);
	if (strlen(g_out) > 7) {
		if (memcmp(g_out + strlen(g_out) - 7, g_carith_suffix, 7) == 0) {
			// remove .carith suffix from output
			g_out[strlen(g_out) - 7] = 0;
			color_debug("stripped g_out of suffix: len %d: %s\n", strlen(g_out), g_out);
		}
	}
	if (g_keep > 0) {
		strcat(g_out, g_keep_suffix);
	}

	g_out_fd = open(g_out, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (g_out_fd < 0) {
		color_err_printf(1, "unable to open output file");
		exit(EXIT_FAILURE);
	}

	if (g_verbose) color_printf("*acarith:*d decompressing to *h%s*d ... ", g_out);

	int l_eof = 0;
	int l_block_ctr = 0;
	int l_docontinue = 0;
	uint32_t l_crc = 0;
	uint32_t l_block_crc = 0;
	uint32_t l_read_compsize;
	uint32_t l_read_totalcompsize;
	uint16_t l_read_freqsize;
	uint32_t l_read_plainsize;
	uint32_t l_read_rleintermediate;

	// spin up and init threads
	for (i = 0; i < g_threads; ++i) {
		pthread_mutex_init(&twa[i].sig_mtx, NULL);
		pthread_cond_init(&twa[i].sig_cond, NULL);
		twa[i].id = i;
		twa[i].runflag = 1;
		pthread_create(&twa[i].thread, NULL, compress_tf, &twa[i]);
	}

	size_t l_sofar = 0;
	if (g_verbose) color_progress(l_sofar, ntohl(l_fh.total_plain_len));

	do {
		g_tally = 0;
		for (i = 0; i < g_threads; ++i) {
			res = read(g_in_fd, &l_read_rleintermediate, sizeof(uint32_t));
			if (res == 0) {
				// eof
				l_eof = 1;
				if (i == 0)
					l_docontinue = 1;
				break;
			}
			if (res < 0) {
				color_err_printf(1, "unable to read input file");
				exit(EXIT_FAILURE);
			}
			if (res < sizeof(uint32_t)) {
				color_err_printf(0, "problems reading input file, read %ld expected to read %ld", res, sizeof(uint32_t));
				exit(EXIT_FAILURE);
			}
			l_read_rleintermediate = ntohl(l_read_rleintermediate);
			res = read(g_in_fd, &l_read_totalcompsize, sizeof(uint32_t));
			if (res < 0) {
				color_err_printf(1, "unable to read input file");
				exit(EXIT_FAILURE);
			}
			if (res < sizeof(uint32_t)) {
				color_err_printf(0, "problems reading input file, read %ld expected to read %ld", res, sizeof(uint32_t));
				exit(EXIT_FAILURE);
			}
			l_read_totalcompsize = ntohl(l_read_totalcompsize);
			res = read(g_in_fd, &l_read_freqsize, sizeof(uint16_t));
			if (res < 0) {
				color_err_printf(1, "unable to read input file");
				exit(EXIT_FAILURE);
			}
			if (res < sizeof(uint16_t)) {
				color_err_printf(0, "problems reading input file, read %ld expected to read %ld", res, sizeof(uint16_t));
				exit(EXIT_FAILURE);
			}
			l_read_freqsize = ntohs(l_read_freqsize);
			l_read_compsize = l_read_totalcompsize - l_read_freqsize;
			res = read(g_in_fd, &l_read_plainsize, sizeof(uint32_t));
			if (res < 0) {
				color_err_printf(1, "unable to read input file");
				exit(EXIT_FAILURE);
			}
			if (res < sizeof(uint32_t)) {
				color_err_printf(0, "problems reading input file, read %ld expected to read %ld", res, sizeof(uint32_t));
				exit(EXIT_FAILURE);
			}
			l_read_plainsize = ntohl(l_read_plainsize);

			color_debug("block %d totalcompsize %d compsize %d freqsize %d rle_intermediate %ld\n", l_block_ctr, l_read_totalcompsize, l_read_compsize, l_read_freqsize, l_read_rleintermediate);
			ctx[i].block_num = l_block_ctr;
			ctx[i].plain_len = l_read_plainsize;
			ctx[i].rle_intermediate = l_read_rleintermediate;
			ctx[i].freq_comp_len = l_read_freqsize;
			res = read(g_in_fd, ctx[i].freq_comp, l_read_freqsize);
			if (res < 0) {
				color_err_printf(1, "unable to read input file");
				exit(EXIT_FAILURE);
			}
			if (res < l_read_freqsize) {
				color_err_printf(0, "problems reading input file, read %ld expected to read %ld", res, l_read_freqsize);
				exit(EXIT_FAILURE);
			}
			ctx[i].comp_len = l_read_compsize;
			res = read(g_in_fd, ctx[i].comp, l_read_compsize);
			if (res < 0) {
				color_err_printf(1, "unable to read input file");
				exit(EXIT_FAILURE);
			}
			if (res < l_read_compsize) {
				color_err_printf(0, "problems reading input file, read %ld expected to read %ld", res, l_read_compsize);
				exit(EXIT_FAILURE);
			}

			// populate a thread and signal it
			ctx[i].scheme = l_fh.scheme;
			pthread_mutex_lock(&twa[i].sig_mtx);
			twa[i].cur_block = l_block_ctr;
			twa[i].sigflag = 1;
			pthread_cond_signal(&twa[i].sig_cond);
			pthread_mutex_unlock(&twa[i].sig_mtx);

			l_block_ctr++;
		}
		if (l_docontinue > 0)
			continue;

		color_debug("waiting for threads to finish\n");
		// wait for threads to finish
		pthread_mutex_lock(&g_tally_mtx);
		while (g_tally < i)
			pthread_cond_wait(&g_tally_cond, &g_tally_mtx);
		pthread_mutex_unlock(&g_tally_mtx);

		// all our threads are done and the plains are all contained in the ctx data structures
		color_debug("processing %d blocks\n", i);
		for (j = 0; j < i; ++j) {
			l_crc = get_buffer_crc(l_crc, ctx[j].decomp, ctx[j].decomp_len);
			l_block_crc = get_buffer_crc(0, ctx[j].decomp, ctx[j].decomp_len);
			color_debug("writing block %d to file. block CRC: %08X\n", ctx[j].block_num, l_block_crc);
			// write plains to file
			res = write(g_out_fd, ctx[j].decomp, ctx[j].decomp_len);
			if (res < 0) {
				color_err_printf(1, "carith: unable to write to output file.");
				exit(EXIT_FAILURE);
			}
			if (res != ctx[j].decomp_len) {
				color_err_printf(0, "carith: difficulty writing to output file: wrote %ld expected to write %ld.", res, ctx[j].decomp_len);
				exit(EXIT_FAILURE);
			}
			l_sofar += ctx[j].decomp_len;
		}
		if (g_verbose) color_progress(l_sofar, ntohl(l_fh.total_plain_len));
	} while (l_eof == 0);

	if (g_verbose) printf("\n");

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

	color_debug("output file CRC: %08X\n", l_crc);
	if (l_crc != htonl(l_fh.plain_crc)) {
		color_printf("*acarith:*d *eCRC mismatch*d, expected *h%08X*d but got *h%08X*d.\n", htonl(l_fh.plain_crc), l_crc);
	} else {
		if (g_verbose) color_printf("*acarith:*d CRC *hOK*d (*h%08X*d)\n", l_crc);
	}

	if (g_keep == 0) {
		// unlink g_in
		unlink(g_in);
	}

	// set mode of output file to whatever the original file had
	res = chmod(g_out, ntohl(l_fh.mode));
	if (res < 0) {
		color_err_printf(1, "unable to chmod output file to original mode");
		exit(EXIT_FAILURE);
	}

	close(g_in_fd);
	close(g_out_fd);
	return;
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
			case OPT_NOKEEP:
			{
				g_keep = 0;
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
				color_printf("*a     (--nokeep)*d delete input files, like UNIX compress command\n");
				color_printf("*hoperational modes*a (choose only one)*d\n");
				color_printf("*a  -c (--compress) <file>*d compress a file\n");
				color_printf("*a  -x (--extract) <file.carith>*d extract a file\n");
				color_printf("*a  -t (--tell) <file.carith>*d show contents of compressed file\n");
				exit(EXIT_SUCCESS);
			}
			break;
		}
	}

	setbuf(stdout, NULL); // disable buffering so we can print our color_progress
	color_init(g_nocolor, g_debug);

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
		if (g_verbose) color_printf("*acarith:*d keep mode: *h%s*d\n", (g_keep ? "YES" : "NO"));
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
		if (g_verbose) color_printf("*acarith:*d keep mode: *h%s*d\n", (g_keep ? "YES" : "NO"));
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
		extract();
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
	color_free();

	gettimeofday(&g_end_time, NULL);
	if (g_verbose) color_printf("*acarith:*d completed operation in *h%ld*d seconds *h%ld*d usecs.\n",
		g_end_time.tv_sec - g_start_time.tv_sec - ((g_end_time.tv_usec - g_start_time.tv_usec < 0) ? 1 : 0), // subtract 1 if there was a usec rollover
		g_end_time.tv_usec - g_start_time.tv_usec + ((g_end_time.tv_usec - g_start_time.tv_usec < 0) ? 1000000 : 0)); // bump usecs by 1 million usec for rollover

	return 0;
}

