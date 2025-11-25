#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <pthread.h>

#include "carith.h"

#define MAXTHREADS 48
#define DEFAULT_SEGSIZE 524288
#define BUFFLEN 1024

struct timeval g_start_time, g_end_time;
int g_debug = 0;
int g_verbose = 0;
int g_norle = 0;
int g_rleonly = 0;
uint32_t g_segsize = DEFAULT_SEGSIZE;
enum { MODE_NONE, MODE_COMPRESS, MODE_EXTRACT, MODE_TELL } g_mode = MODE_NONE;
char g_in[BUFFLEN];
char g_out[BUFFLEN];

// concurrency
int g_threads = 8; // default thread count

typedef struct {
	pthread_t thread;
	unsigned int id;
	int runflag;
	pthread_mutex_t sig_mtx;
	int sigflag;
	pthread_cond_t sig_cond;
	carith_comp_ctx ctx;
} thread_work_area;

thread_work_area twa[MAXTHREADS];

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

void color_printf(const char *format, ...)
{
	char edited_format[1024];
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
	char edited_format[1024];
	edited_format[0] = 0;
	if (!g_nocolor) strcat(edited_format, CCCT_COLOR_ERROR);
	strcat(edited_format, format);
	if (a_strerror) {
		sprintf(edited_format + strlen(edited_format), ": %s\n", strerror(errno));
	} else {
		sprintf(edited_format + strlen(edited_format), "\n");
	}
	if (!g_nocolor) strcat(edited_format, CCCT_COLOR_DEFAULT);
	va_list args;
	va_start(args, format);
	vfprintf(stderr, edited_format, args);
	va_end(args);
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
}

void compress()
{
	// compress file g_in
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
		init_error = carith_init_ctx(&twa[i].ctx, g_segsize);
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
		if (g_verbose) color_printf("*acarith:*d compressing *h%s*d\n", argv[optind]);
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
	} else if (g_mode == MODE_TELL) {
		if (optind >= argc) {
			color_err_printf(0, "carith: expected file argument.");
			exit(EXIT_FAILURE);
		}
		if (g_verbose) color_printf("*acarith:*d telling contents of *h%s*d\n", argv[optind]);
	} else {
		color_err_printf(0, "carith: please choose at least one operational mode.");
		color_err_printf(0, "carith: use -? or --help for usage information.");
		exit(EXIT_FAILURE);
	}

	// free
	for (i = 0; i < g_threads; ++i) {
		carith_free_ctx(&twa[i].ctx);
	}

	gettimeofday(&g_end_time, NULL);
	if (g_verbose) color_printf("*acarith:*d completed operation in *h%ld*d seconds *h%ld*d usecs.\n",
		g_end_time.tv_sec - g_start_time.tv_sec - ((g_end_time.tv_usec - g_start_time.tv_usec < 0) ? 1 : 0), // subtract 1 if there was a usec rollover
		g_end_time.tv_usec - g_start_time.tv_usec + ((g_end_time.tv_usec - g_start_time.tv_usec < 0) ? 1000000 : 0)); // bump usecs by 1 million usec for rollover

	return 0;
}

