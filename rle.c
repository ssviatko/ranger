/**
 *
 * Run Length Encoder/Decoder API
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
 * @file rle.c
 * @brief Run Length Encoder API
 *
 * Compresses data by removing runs of repeated characters.
 *
 */

#include "rle.h"

void rle_encode(uint8_t *a_in, uint8_t *a_out, size_t a_insize, size_t *a_outsize)
{
    uint8_t RLE_ESCAPE = 0x55;
    const uint8_t RLE_INCREMENT = 0x3B;
    int l_eof = 0;
    size_t i;
    uint8_t l_new, l_old = 0, l_count = 0;
    int l_clear = 1; // set this to indicate l_old is empty
    size_t inptr = 0;
    size_t outptr = 0;

    // sliding window RLE
    do {
        l_new = a_in[inptr++];
        if (inptr == a_insize) {
            l_eof = 1;
        }

        // did we encounter an escape? If so, flush the window then double it up, then rotate the escape
        if (l_new == RLE_ESCAPE) {
            if (l_clear == 0) { // if we have an l_old value trucking along with us
                if (l_count > 0) {
                    // if we've repeated fewer than 4 times, just write the bytes themselves
                    if (l_count < 3) {
                        for (i = 0; i <= l_count; ++i) {
                            a_out[outptr++] = l_old;
                        }
                        l_count = 0;
                        // after writing out our repeated l_olds, write out double escape then clear
                        a_out[outptr++] = RLE_ESCAPE;
                        a_out[outptr++] = RLE_ESCAPE;
                        RLE_ESCAPE += RLE_INCREMENT;
                        l_clear = 1;
                    } else {
                        // write out compound set
                        a_out[outptr++] = RLE_ESCAPE;
                        a_out[outptr++] = l_old;
                        uint8_t lc1 = l_count + 1;
                        a_out[outptr++] = lc1;
                        RLE_ESCAPE += RLE_INCREMENT;
                        l_count = 0;
                        // we rotated our escape after writing our run, so l_new is no longer the escape character, so just write it out
                        a_out[outptr++] = l_new;
                        l_clear = 1;
                    }
                } else {
                    // weren't counting when we encountered escape, so write out l_old, double escape, then start fresh
                    a_out[outptr++] = l_old;
                    a_out[outptr++] = RLE_ESCAPE;
                    a_out[outptr++] = RLE_ESCAPE;
                    RLE_ESCAPE += RLE_INCREMENT;
                    l_clear = 1;
                }
            } else { // no l_old value, encountered a fresh escape character
                a_out[outptr++] = RLE_ESCAPE;
                a_out[outptr++] = RLE_ESCAPE;
                RLE_ESCAPE += RLE_INCREMENT;
                l_clear = 1;
            }
            continue;
        }

        // first time through the loop (and after escapes), just stash away the first byte
        if (l_clear == 1) {
            l_old = l_new;
            l_clear = 0;
            continue;
        }

        if (l_old == l_new) {
            ++l_count;
            if (l_count == 254) { // 254 repeats = 255 characters
                // flush window and restart the count if we reached count limit
                a_out[outptr++] = RLE_ESCAPE;
                a_out[outptr++] = l_old;
                uint8_t lc1 = l_count + 1;
                a_out[outptr++] = lc1;
                RLE_ESCAPE += RLE_INCREMENT;
                l_count = 0;
                l_clear = 1;
            }
        } else { // l_old and l_new differ
            if (l_count > 0) {
                // if we've repeated fewer than 4 times, just write the bytes themselves
                if (l_count < 3) {
                    for (i = 0; i <= l_count; ++i) {
                        a_out[outptr++] = l_old;
                    }
                    l_count = 0;
                } else {
                    // write out compound set
                    a_out[outptr++] = RLE_ESCAPE;
                    a_out[outptr++] = l_old;
                    uint8_t lc1 = l_count + 1;
                    a_out[outptr++] = lc1;
                    RLE_ESCAPE += RLE_INCREMENT;
                    l_count = 0;
                    // edge case: is l_new now the escape character after above compound set write?
                    // write out l_new twice, increment the escape again, and then start fresh
                    if (l_new == RLE_ESCAPE) {
                        a_out[outptr++] = RLE_ESCAPE;
                        a_out[outptr++] = RLE_ESCAPE;
                        RLE_ESCAPE += RLE_INCREMENT;
                        l_clear = 1;
                        continue;
                    }

                }
            } else {
                // got different byte, but no repeat, so write out old
                a_out[outptr++] = l_old;
            }
            l_old = l_new;
        }
    } while (l_eof == 0);

    // flush window
    if (!l_clear) {
        if (l_count > 0) {
            if (l_count < 3) {
                for (i = 0; i <= l_count; ++i) {
                    a_out[outptr++] = l_old;
                }
                l_count = 0;
            } else {
                // write out compound set
                a_out[outptr++] = RLE_ESCAPE;
                a_out[outptr++] = l_old;
                uint8_t lc1 = l_count + 1;
                a_out[outptr++] = lc1;
                RLE_ESCAPE += RLE_INCREMENT;
                l_count = 0;
            }
        } else {
            a_out[outptr++] = l_old;
        }
    }
    *a_outsize = outptr;
    return;
}

void rle_decode(uint8_t *a_in, uint8_t *a_out, size_t a_insize, size_t *a_outsize)
{
    uint8_t RLE_ESCAPE = 0x55;
    const uint8_t RLE_INCREMENT = 0x3B;
    int l_eof = 0;
    size_t inptr = 0;
    size_t outptr = 0;
    uint64_t l_repeat = 0;
    size_t i;
    enum { COLLECTING, FOUND_ESCAPE, FOUND_CHAR } l_state = COLLECTING;

    do {
        uint8_t l_new = a_in[inptr++];
        if (inptr == a_insize) {
            l_eof = 1;
        }
        switch (l_state) {
            case COLLECTING:
                if (l_new == RLE_ESCAPE) {
                    l_state = FOUND_ESCAPE;
                } else {
                    // just a normal char, so write it out
                    a_out[outptr++] = l_new;
                }
                break;
            case FOUND_ESCAPE:
                if (l_new == RLE_ESCAPE) {
                    // found second escape character, so write it then rotate escape
                    a_out[outptr++] = l_new;
                    RLE_ESCAPE += RLE_INCREMENT;
                    l_state = COLLECTING;
                } else {
                    // something else, must be the char we need to repeat
                    l_repeat = l_new;
                    l_state = FOUND_CHAR;
                }
                break;
            case FOUND_CHAR:
                if (l_new > 0) {
                    for (i = 0; i < l_new; ++i) {
                        a_out[outptr++] = l_repeat;
                    }
                    RLE_ESCAPE += RLE_INCREMENT;
                    l_state = COLLECTING;
                } else {
                    // value of 0 is illegali in a repeat construct, so data stream must be corrupted
                    fprintf(stderr, "rle_decode: Illegal character in stream, possible data corruption.\n");
                    exit(EXIT_FAILURE);
                }
                break;
        }
    } while (l_eof == 0);
    *a_outsize = outptr;
    return;
}
