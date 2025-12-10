/**
 *
 * C Terminal Color Printing API
 * 2025/Nov/23 - Revision 0.80 alpha
 *
 * Created by: Stephen Sviatko
 *
 * (C) 2025 Good Neighbors LLC - All Rights Reserved, except where noted
 *
 * This file and any intellectual property (designs, algorithms, formulas,
 * procedures, trademarks, and related documentation) contained herein are
 * property of Good Neighbors, an Arizona Limited Liability Company.
 *
 * LICENSING INFORMATION
 *
 * This file may not be distributed in any modified form without expressed
 * written permission of Good Neighbors LLC or its regents. Permission is
 * granted to use this file in any non-commercial, non-governmental capacity
 * (such as student projects, hobby projects, etc) without an official
 * licensing agreement as long as the original author(s) are credited in any
 * derivative work.
 *
 * Commercial licensing of this content is available, any agreement must
 * include consulting services as part of a deployment strategy. For more
 * information, please contact Stephen Sviatko at the following email address:
 *
 * ssviatko@gmail.com
 *
 * @file color_print.c
 * @brief C Terminal Color Printing API
 *
 * Extends printf and fprintf to enable color ANSI printing to the terminal.
 *
 */

#include "color_print.h"

static char g_ansi_highlight[ANSIBUFFLEN];
static char g_ansi_heading[ANSIBUFFLEN];
static char g_ansi_error[ANSIBUFFLEN];
static char g_ansi_default[ANSIBUFFLEN];
static char g_ansi_bullet[ANSIBUFFLEN];
static char g_ansi_debug[ANSIBUFFLEN];

static int g_nocolor; ///< Set to 1 to disable color printing
static int g_debug;   ///< Set to 1 to enable debug printing
static pthread_mutex_t g_debug_mtx; ///< protect debug messages in multithreaded environment

void color_init(const int a_nocolor, const int a_debug)
{
    g_nocolor = a_nocolor;
    g_debug = a_debug;
    pthread_mutex_init(&g_debug_mtx, NULL);
}

void color_set_nocolor(const int a_nocolor)
{
    g_nocolor = a_nocolor;
}

void color_set_debug(const int a_debug)
{
    g_debug = a_debug;
}

void color_set_theme(cp_theme_t a_theme)
{
    g_ansi_highlight[0] = 0;
    g_ansi_heading[0] = 0;
    g_ansi_error[0] = 0;
    g_ansi_debug[0] = 0;
    g_ansi_bullet[0] = 0;
    g_ansi_default[0] = 0;

    switch (a_theme) {
        default:
        case THEME_GREEN:
        {
            strcpy(g_ansi_highlight, CP_GREEN_COLOR_HIGHLIGHT);
            strcpy(g_ansi_heading, CP_GREEN_COLOR_HEADING);
            strcpy(g_ansi_error, CP_GREEN_COLOR_ERROR);
            strcpy(g_ansi_default, CP_GREEN_COLOR_DEFAULT);
            strcpy(g_ansi_debug, CP_GREEN_COLOR_DEBUG);
            strcpy(g_ansi_bullet, CP_GREEN_COLOR_BULLET);
        }
        break;
        case THEME_BLUE:
        {
            strcpy(g_ansi_highlight, CP_BLUE_COLOR_HIGHLIGHT);
            strcpy(g_ansi_heading, CP_BLUE_COLOR_HEADING);
            strcpy(g_ansi_error, CP_BLUE_COLOR_ERROR);
            strcpy(g_ansi_default, CP_BLUE_COLOR_DEFAULT);
            strcpy(g_ansi_debug, CP_BLUE_COLOR_DEBUG);
            strcpy(g_ansi_bullet, CP_BLUE_COLOR_BULLET);
        }
        break;
        case THEME_RED:
        {
            strcpy(g_ansi_highlight, CP_RED_COLOR_HIGHLIGHT);
            strcpy(g_ansi_heading, CP_RED_COLOR_HEADING);
            strcpy(g_ansi_error, CP_RED_COLOR_ERROR);
            strcpy(g_ansi_default, CP_RED_COLOR_DEFAULT);
            strcpy(g_ansi_debug, CP_RED_COLOR_DEBUG);
            strcpy(g_ansi_bullet, CP_RED_COLOR_BULLET);
        }
        break;
        case THEME_PURPLE:
        {
            strcpy(g_ansi_highlight, CP_PURPLE_COLOR_HIGHLIGHT);
            strcpy(g_ansi_heading, CP_PURPLE_COLOR_HEADING);
            strcpy(g_ansi_error, CP_PURPLE_COLOR_ERROR);
            strcpy(g_ansi_default, CP_PURPLE_COLOR_DEFAULT);
            strcpy(g_ansi_debug, CP_PURPLE_COLOR_DEBUG);
            strcpy(g_ansi_bullet, CP_PURPLE_COLOR_BULLET);
        }
        break;
    }
}

void color_free()
{
    pthread_mutex_destroy(&g_debug_mtx);
}

void color_progress(uint32_t a_sofar, uint32_t a_total)
{
    static size_t l_lastsize = 0;
    int i;
    char l_txt[BUFFLEN];

    if (a_sofar == 0)
        l_lastsize = 0; // start fresh

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
    color_printf("(*h%u*d of *h%u*d) ", a_sofar, a_total);
}

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
                    strcat(edited_format, g_ansi_highlight);
                    j += strlen(g_ansi_highlight);
                }
            } else if (c == 'a') {
                if (!g_nocolor) {
                    edited_format[j] = 0;
                    strcat(edited_format, g_ansi_heading);
                    j += strlen(g_ansi_heading);
                }
            } else if (c == 'b') {
                if (!g_nocolor) {
                    edited_format[j] = 0;
                    strcat(edited_format, g_ansi_bullet);
                    j += strlen(g_ansi_bullet);
                }
            } else if (c == 'e') {
                if (!g_nocolor) {
                    edited_format[j] = 0;
                    strcat(edited_format, g_ansi_error);
                    j += strlen(g_ansi_error);
                }
            } else if (c == 'd') {
                if (!g_nocolor) {
                    edited_format[j] = 0;
                    strcat(edited_format, g_ansi_default);
                    j += strlen(g_ansi_default);
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
    if (!g_nocolor) strcat(edited_format, g_ansi_error);
    strcat(edited_format, format);
    if (a_strerror) {
        sprintf(edited_format + strlen(edited_format), " : %s\n", strerror(errno));
    } else {
        sprintf(edited_format + strlen(edited_format), "\n");
    }
    if (!g_nocolor) strcat(edited_format, g_ansi_default);
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
        strcat(edited_format, g_ansi_debug);
    strcat(edited_format, format);
    if (!g_nocolor)
        strcat(edited_format, g_ansi_default);
    va_list args;
    va_start(args, format);
    vprintf(edited_format, args);
    va_end(args);
    pthread_mutex_unlock(&g_debug_mtx);
}
