#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "carith.h"

int g_debug = 0;

enum {
	OPT_DEBUG = 1001,
	OPT_NOCOLOR
};

struct option g_options[] = {
	{ "help", no_argument, NULL, '?' },
	{ "debug", no_argument, NULL, OPT_DEBUG },
	{ "nocolor", no_argument, NULL, OPT_NOCOLOR },
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

int main(int argc, char **argv)
{
	int opt;

	while ((opt = getopt_long(argc, argv, "i:o:k:g:edsv?twbcf:", g_options, NULL)) != -1) {
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
			case '?':
			{
				color_printf("*hcarith (C arithmetic coder) compression utility*d\n");
				color_printf("build *h%s*d release *h%s*d built on *h%s*d\n", BUILD_NUMBER, RELEASE_NUMBER, BUILD_DATE);
				color_printf("*aby Stephen Sviatko - (C) 2025 Good Neighbors LLC*d\n");
				color_printf("*ausage: carith <options>*d\n");
				color_printf("*a  -? (--help)*d this screen\n");
				color_printf("*a     (--debug)*d enable debug mode\n");
				color_printf("*a     (--nocolor)*d defeat colors\n");
				exit(EXIT_SUCCESS);
			}
			break;
		}
	}

	setbuf(stdout, NULL); // disable buffering so we can print our ccct_progress

	return 0;
}

