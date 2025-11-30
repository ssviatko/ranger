/**
 *
 * C Arithmetic Coder
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
 * @file carith.h
 * @brief C Arithmetic Coder API
 *
 * This file implements the core functionalty of the C Arithmetic Coder.
 *
 */

#include "carith.h"

const char *carith_error_string[] = {
    "none",
    "memory allocation error"
}; ///< List of standard carith error strings correlated to integer carith error codes.

static void freq_count(carith_comp_ctx *ctx, uint8_t *a_buff, size_t a_source_size)
{
    size_t i;
    uint64_t base_tab = 0;

    for (i = 0; i < 256; ++i) {
        ctx->freq[i].count_base = 0;
        ctx->freq[i].count = 0;
    }
    for (i = 0; i < a_source_size; ++i) {
        ctx->freq[a_buff[i]].count++;
    }
    for (i = 0; i < 256; ++i) {
        ctx->freq[i].count_base = base_tab;
        base_tab += ctx->freq[i].count;
    }
}

static void retrieve_range(carith_comp_ctx *ctx, uint8_t a_token, uint64_t *a_start, uint64_t *a_end, size_t a_source_size)
{
    uint64_t l_rangesize = *a_end - *a_start;
    uint64_t l_start_orig = *a_start;

    //	printf("rr: passed %016lX %016lX ", *a_start, *a_end);
    *a_start = (__uint128_t)l_start_orig + ((__uint128_t)ctx->freq[a_token].count_base * (__uint128_t)l_rangesize) / (__uint128_t)a_source_size;
    *a_end = (__uint128_t)l_start_orig + ((((__uint128_t)ctx->freq[a_token].count_base + (__uint128_t)ctx->freq[a_token].count) * (__uint128_t)l_rangesize) / (__uint128_t)a_source_size) - 1;
    //	printf("assigned token %02X - %ld/%ld/%ld  %016lX %016lX\n", a_token, ctx->freq[a_token].count_base, ctx->freq[a_token].count, ctx->plain_len, *a_start, *a_end);
}

static uint8_t token_for_window(carith_comp_ctx *ctx, uint64_t a_window, uint64_t a_start, uint64_t a_end, size_t a_source_size)
{
    size_t i;
    uint64_t l_rangesize = a_end - a_start;
    uint64_t l_windowpos = a_window - a_start;
    uint64_t l_countpos = ((__uint128_t)l_windowpos * (__uint128_t)a_source_size) / (__uint128_t)l_rangesize;
    //	printf("l_rangesize %016lX l_windowpos %016lX l_countpos %ld plain_len %ld   ", l_rangesize, l_windowpos, l_countpos, ctx->plain_len);
    for (i = 0; i < 256; ++i) {
        //			printf("i %ld %ld %ld\n", i, ctx->freq[i].count_base, ctx->freq[i].count_base + ctx->freq[i].count);
        if ((l_countpos >= ctx->freq[i].count_base) && (l_countpos < ctx->freq[i].count_base + ctx->freq[i].count))
            break;
    }
    // check it to make sure, due to inaccuracies
    uint64_t l_start = a_start;
    uint64_t l_end = a_end;
    retrieve_range(ctx, i, &l_start, &l_end, a_source_size);
    //	printf("checking range for %02lX: %016lX %016lX\n", i, l_start, l_end);
    if (a_window > l_end) {
        ++l_countpos;
        size_t j;
        for (j = 0; j < 256; ++j) {
            //			printf("i %ld %ld %ld\n", i, ctx->freq[i].count_base, ctx->freq[i].count_base + ctx->freq[i].count);
            if ((l_countpos >= ctx->freq[j].count_base) && (l_countpos < ctx->freq[j].count_base + ctx->freq[j].count))
                break;
        }
        //		printf("found substitute %02lx\n", j);
        if (j == 256) {
            // fatal error
        }
        l_start = a_start;
        l_end = a_end;
        retrieve_range(ctx, j, &l_start, &l_end, a_source_size);
        //		printf("j range for %02lX: %016lX %016lX\n", j, l_start, l_end);
        if ((a_window < l_start) || (a_window > l_end)) {
            // fatal error
        }
        i = j;
    }
    // i should equal the token we are looking for
    //	printf("discovered window %016lX conforms to %02lX\n", a_window, i);
    return i;
}

static void rle_encode(uint8_t *a_in, uint8_t *a_out, size_t a_insize, size_t *a_outsize)
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

static void rle_decode(uint8_t *a_in, uint8_t *a_out, size_t a_insize, size_t *a_outsize)
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

/**
 * @brief Returns a char pointer to an existing error string
 * Works in exactly the same way as the strerror(errno) function works in the standard library
 *
 * @param[in] a_errno The numerical error returned by the function
 * @return character pointer to error message
 */

const char *carith_strerror(carith_error_t a_errno)
{
    return carith_error_string[a_errno];
}

/**
 * @brief Initialize a carith context
 * Must be called before any other operations are attempted. This function
 * allocates space for the internal buffers in the carith context.
 *
 * @param[in] ctx Pointer to a carith context object pointer
 * @param[in] a_worksize Size in bytes of requested compression segment
 */

carith_error_t carith_init_ctx(carith_comp_ctx *ctx, size_t a_worksize)
{
    ctx->plain = NULL;
    ctx->plain = malloc(a_worksize);
    if (ctx->plain == NULL) {
        return CARITH_ERR_MEMORY;
    }
    ctx->rleenc = NULL;
    ctx->rleenc = malloc(a_worksize * 3 / 2); // plain guard size 150%
    if (ctx->rleenc == NULL) {
        free(ctx->plain);
        return CARITH_ERR_MEMORY;
    }
    ctx->rledec = NULL;
    ctx->rledec = malloc(a_worksize * 3 / 2); // plain guard size 150%
    if (ctx->rledec == NULL) {
        free(ctx->plain);
        free(ctx->rleenc);
        return CARITH_ERR_MEMORY;
    }
    ctx->comp = NULL;
    ctx->comp = malloc(a_worksize * 3 / 2); // comp guard size 150%
    if (ctx->comp == NULL) {
        free(ctx->plain);
        free(ctx->rleenc);
        free(ctx->rledec);
        return CARITH_ERR_MEMORY;
    }
    ctx->decomp = NULL;
    ctx->decomp = malloc(a_worksize);
    if (ctx->decomp == NULL) {
        free(ctx->plain);
        free(ctx->rleenc);
        free(ctx->rledec);
        free(ctx->comp);
        return CARITH_ERR_MEMORY;
    }
    return CARITH_ERR_NONE;
}

/**
 * @brief Free a carith context
 * Release all allocated memory.
 */

carith_error_t carith_free_ctx(carith_comp_ctx *ctx)
{
    free(ctx->plain);
    free(ctx->rleenc);
    free(ctx->rledec);
    free(ctx->comp);
    free(ctx->decomp);
    return CARITH_ERR_NONE;
}

/**
 * @brief Compress plain buffer into comp buffer
 */

void ccct_print_hex(uint8_t *a_buffer, size_t a_len)
{
    unsigned int g_col = 180;
    unsigned int i;
    unsigned int l_bytes_to_print = (g_col / 48) * 16;
    for (i = 0; i < a_len; ++i) {
        if (i % l_bytes_to_print == 0)
            printf("\n");
        printf("%02X ", a_buffer[i]);
    }
    printf("\n");
}

carith_error_t carith_compress(carith_comp_ctx *ctx)
{
    size_t plain_ptr;
    uint64_t range_lo, range_hi;
    uint8_t cur_byte;
    uint8_t range_lo_hibyte, range_hi_hibyte; // bits 56-63 of the range
    size_t comp_ptr = 0;
    size_t i;
    uint8_t *ac_source = ctx->plain;
    size_t ac_source_size = ctx->plain_len;

//    // sanity check
//    uint8_t sanity[1048576];
//    size_t sanity_count;

    if ((ctx->scheme & scheme_rle) == scheme_rle) {
        // rle encode plain to rleenc
        rle_encode(ctx->plain, ctx->rleenc, ctx->plain_len, &ctx->rle_intermediate);
//        rle_decode(ctx->comp, sanity, ctx->rle_intermediate, &sanity_count);
//        printf("\nsanity check: plain_len %ld rle_intermediate %ld sanity_count %ld memcmp %d\n", ctx->plain_len, ctx->rle_intermediate, sanity_count, memcmp(sanity, ctx->plain, ctx->plain_len));
//        printf("plain: ");
//        ccct_print_hex(ctx->plain, ctx->plain_len);
//        printf("comp: ");
//        ccct_print_hex(ctx->comp, ctx->rle_intermediate);
//        printf("sanity: ");
//        ccct_print_hex(sanity, sanity_count);
//        printf("\n");
        // are we progressing on to AC? if not, finalize the context here and return
        if ((ctx->scheme & scheme_ac) == 0) {
            // RLE but no AC
            // RLE encoded data is in comp, rle_intermediate contains compressed length
            // so set comp_len and freq_comp_len pointers appropriately
            memcpy(ctx->comp, ctx->rleenc, ctx->rle_intermediate);
            ctx->freq_comp_len = 0;
            ctx->comp_len = ctx->rle_intermediate;
            // and return
            return CARITH_ERR_NONE;
        } else {
            // RLE and AC
            ac_source = ctx->rleenc;
            ac_source_size = ctx->rle_intermediate;
        }
    }

    freq_count(ctx, ac_source, ac_source_size);

    range_lo = 0;
    range_hi = ULLONG_MAX;

    for (plain_ptr = 0; plain_ptr < ac_source_size; ++plain_ptr) {
        cur_byte = ac_source[plain_ptr];
        //		range_lo = ctx->freq[cur_byte].range_start;
        //		range_hi = ctx->freq[cur_byte].range_end;
        retrieve_range(ctx, cur_byte, &range_lo, &range_hi, ac_source_size);
        //		printf("pos %ld read %02X new range_lo %010lX range_hi %010lX\n", plain_ptr, cur_byte, range_lo, range_hi);
        range_lo_hibyte = (range_lo >> 56);
        range_hi_hibyte = (range_hi >> 56);
        while (range_lo_hibyte == range_hi_hibyte) {
//			printf("outputing pos %ld byte %02X\n", comp_ptr, range_lo_hibyte);
            ctx->comp[comp_ptr++] = range_lo_hibyte;
            range_lo <<= 8;
            range_hi <<= 8;
            range_hi |= 0xff;
            range_lo_hibyte = (range_lo >> 56);
            range_hi_hibyte = (range_hi >> 56);
        }
        //		assign_ranges(ctx, range_lo, range_hi);
    }
    //	uint64_t l_mid = (range_hi + range_lo) / 2;
    uint64_t l_mid = range_lo;
    //	printf("hi %016lX lo %016lx mid %016lx\n", range_hi, range_lo, l_mid);
    for (i = 0; i < 8; ++i) {
        range_lo_hibyte = (l_mid >> 56) & 0xff;
//        printf("comp pos %ld outputting final 5-byte word %02X\n", comp_ptr, range_lo_hibyte);
        ctx->comp[comp_ptr++] = range_lo_hibyte;
        l_mid <<= 8;
    }
    ctx->comp_len = comp_ptr;

//    printf("process: compressed %ld bytes into %ld.\n", ctx->plain_len, ctx->comp_len);

    uint8_t ftbl_enum[1024];
    memset(ftbl_enum, 0, 1024);
    uint16_t ftbl_enum_len;
    uint16_t ftbl_enum_entries = 0;
    uint8_t ftbl_full[1024];
    memset(ftbl_full, 0, 1024);
    uint16_t ftbl_full_len;
    uint64_t countmax = 0;
    for (i = 0; i < 256; ++i) {
        if (ctx->freq[i].count > countmax)
            countmax = ctx->freq[i].count;
    }
    uint16_t countwidth = cbit_bit_width(countmax);
    cbit_cursor_t bc;

    // construct enumerated frequency table
    bc.byte = 0;
    bc.bit = 7;
    bc.buffer = ftbl_enum;
    cbit_write(&bc, 1); // first bit true indicates it's enumerated
    cbit_write_many(&bc, countwidth, 5); // value 0-31 for countwidth
    for (i = 0; i < 256; ++i) {
        if (ctx->freq[i].count > 0)
            ftbl_enum_entries++;
    }
    cbit_write_many(&bc, ftbl_enum_entries, 9); // 0-256 number of active symbols
    for (i = 0; i < 256; ++i) {
        if (ctx->freq[i].count > 0) {
            cbit_write_many(&bc, i, 8);
            cbit_write_many(&bc, ctx->freq[i].count, countwidth);
        }
    }
    if (bc.bit < 7) {
        bc.byte++;
        bc.bit = 7;
    }
    ftbl_enum_len = bc.byte;
    //	printf("enumerated frequency table size: %d\n", ftbl_enum_len);

    // construct full frequency table
    bc.byte = 0;
    bc.bit = 7;
    bc.buffer = ftbl_full;
    cbit_write(&bc, 0); // first bit false indicates it's full
    cbit_write_many(&bc, countwidth, 5); // value 0-31 for countwidth
    for (i = 0; i < 256; ++i) {
        cbit_write_many(&bc, ctx->freq[i].count, countwidth);
    }
    if (bc.bit < 7) {
        bc.byte++;
        bc.bit = 7;
    }
    ftbl_full_len = bc.byte;
    //	printf("full frequency table size: %d\n", ftbl_full_len);

    if (ftbl_enum_len < ftbl_full_len) {
        memcpy(ctx->freq_comp, ftbl_enum, ftbl_enum_len);
        ctx->freq_comp_len = ftbl_enum_len;
    } else {
        memcpy(ctx->freq_comp, ftbl_full, ftbl_full_len);
        ctx->freq_comp_len = ftbl_full_len;
    }
    //	printf("compressed frequency table length: %d\n", ctx->freq_comp_len);
//    uint64_t total_comp_len = ctx->freq_comp_len + ctx->comp_len;
//    printf("after compress: plain_len %ld rle_intermediate %ld comp_len %ld\n", ctx->plain_len, ctx->rle_intermediate, ctx->comp_len);

    return CARITH_ERR_NONE;
}

/**
 * @brief Decompress comp buffer into decomp buffer
 */

carith_error_t carith_extract(carith_comp_ctx *ctx)
{
    uint8_t *ac_dest = ctx->decomp;
    size_t ac_source_size = ctx->plain_len;
    size_t *ac_dest_size;
    ac_dest_size = &ctx->decomp_len;

     // if we're doing RLE only, just skip all the AC stuff
    if ((ctx->scheme & 0xc0) == scheme_rle)
        goto carith_extract_skipac;

    // if we're using AC and RLE, change ac_dest to point to rledec
    if ((ctx->scheme & scheme_rle) == scheme_rle) {
        ac_dest = ctx->rledec;
        ac_dest_size = &ctx->rledec_len;
        ac_source_size = ctx->rle_intermediate;
    }

    uint64_t range_lo, range_hi;
    cbit_cursor_t bc;
    uint8_t range_lo_hibyte, range_hi_hibyte; // bits 32-40 of the range
    size_t comp_ptr, decomp_ptr;
    size_t i;
    uint64_t window;
    uint16_t countwidth;
    uint16_t ftbl_enum_entries;

    // obliterate frequency table
    for (i = 0; i < 256; ++i) {
        ctx->freq[i].count = 0;
        ctx->freq[i].count_base = 0;
    }

    // read compressed frequency table
    uint64_t base_tab = 0;
    bc.byte = 0;
    bc.bit = 7;
    bc.buffer = ctx->freq_comp;;
    int ftbl_type = cbit_read(&bc);
    if (ftbl_type == 1) {
        countwidth = cbit_read_many(&bc, 5);
        ftbl_enum_entries = cbit_read_many(&bc, 9);
        //		printf("read compressed table - countwidth %d ftbl_enum_entries %ld\n", countwidth, ftbl_enum_entries);
        for (i = 0; i < ftbl_enum_entries; ++i) {
            uint8_t symbol = cbit_read_many(&bc, 8);
            uint64_t symbol_count = cbit_read_many(&bc, countwidth);
            ctx->freq[symbol].count_base = base_tab;
            ctx->freq[symbol].count = symbol_count;
            base_tab += symbol_count;
        }
    } else {
        countwidth = cbit_read_many(&bc, 5);
        for (i = 0; i < 256; ++i) {
            ctx->freq[i].count_base = base_tab;
            uint64_t symbol_count = cbit_read_many(&bc, countwidth);
            ctx->freq[i].count = symbol_count;
            base_tab += symbol_count;
        }
    }

    // change decomp to plain once this is debugged and tested
    range_lo = 0;
    range_hi = ULLONG_MAX;
    //	assign_ranges(ctx, range_lo, range_hi);
    comp_ptr = 0;
    decomp_ptr = 0;
    // prime the pump
    window = 0;
    for (i = 0; i < 8; ++i) {
        window <<= 8;
        window |= ctx->comp[comp_ptr++];
    }

    while (1) {
        //		assign_ranges(ctx, range_lo, range_hi);
        //		found = 0; // keep track of whether or not we found the range. If we didn't find the range, this is a fatal error!
        //		printf("comp_ptr %ld decomp_ptr %ld\n", comp_ptr, decomp_ptr);
        // find which range window falls between
        //		uint64_t l_rangesize = range_hi - range_lo;
        //		uint64_t l_windowpos = window - range_lo;
        //		uint64_t l_countpos = ((__uint128_t)l_windowpos * (__uint128_t)ctx->plain_len) / (__uint128_t)l_rangesize;
        //		printf("l_rangesize %016lX l_windowpos %016lX l_countpos %ld plain_len %ld\n", l_rangesize, l_windowpos, (uint64_t)l_countpos, ctx->plain_len);
        //		for (i = 0; i < 256; ++i) {
        //			printf("i %ld %ld %ld\n", i, ctx->freq[i].count_base, ctx->freq[i].count_base + ctx->freq[i].count);
        //			if ((l_countpos >= ctx->freq[i].count_base) && (l_countpos < ctx->freq[i].count_base + ctx->freq[i].count))
        //				break;
        //		}
        i = token_for_window(ctx, window, range_lo, range_hi, ac_source_size);
        // i should equal the token we are looking for
        //		printf("discovered window %016lX conforms to %02lX\n", window, i);
        //		printf("%ld\n", i);
        // output i to decomp stream
        //		printf("decomp_ptr %ld outputting %02lX\n", decomp_ptr, i);
        ac_dest[decomp_ptr++] = i;
        retrieve_range(ctx, i, &range_lo, &range_hi, ac_source_size);
        //		printf("new range %016lX %016lX\n", range_lo, range_hi);
        //		found = 1;
        //		range_lo = ctx->freq[i].range_start;
        //		range_hi = ctx->freq[i].range_end;
        range_lo_hibyte = (range_lo >> 56);
        range_hi_hibyte = (range_hi >> 56);
        while (range_lo_hibyte == range_hi_hibyte) {
            //			printf("hibyte equivalency %02X range_lo %010lX range_hi %010lx\n", range_lo_hibyte, range_lo, range_hi);
            // scoot our ranges over by 8 bits
            range_lo <<= 8;
            range_hi <<= 8;
            range_hi |= 0xff;
            // slide our window over and read next compressed byte into the low position
            window <<= 8;
            window |= ctx->comp[comp_ptr++];
            // refresh our hibyte values for the next test at top of loop
            range_lo_hibyte = (range_lo >> 56);
            range_hi_hibyte = (range_hi >> 56);
            //			printf("scoot     %016lX %016lX\n", range_lo, range_hi);
        }
        //			assign_ranges(ctx, range_lo, range_hi);
        //		if (found == 0) {
        //			printf("range not found! %010lX\n", window);
        //			exit(EXIT_FAILURE);
        //		}
        if (decomp_ptr >= ac_source_size)
            break;
    }
    *ac_dest_size = decomp_ptr;

    // if we're doing AC only, just return
    if ((ctx->scheme & 0xc0) == scheme_ac)
        return CARITH_ERR_NONE;

carith_extract_skipac:

    // if we did rle only, copy comp into rledec
    if ((ctx->scheme & 0xc0) == scheme_rle) {
        memcpy(ctx->rledec, ctx->comp, ctx->comp_len);
        ctx->rledec_len = ctx->comp_len;
    }

    // just for laughs, lets verify that rleenc and rledec contain the same data
//    printf("rleenc == rledec %d\n", memcmp(ctx->rleenc, ctx->rledec, ctx->rle_intermediate));
//    if (memcmp(ctx->rleenc, ctx->rledec, ctx->rle_intermediate) != 0) {
//        printf("rleenc");
//        ccct_print_hex(ctx->rleenc, ctx->rle_intermediate);
//        printf("rledec");
//        ccct_print_hex(ctx->rledec, ctx->rle_intermediate);
//    }
//    printf("after extract: comp_len %ld rledec_len %ld rle_intermediate %ld\n", ctx->comp_len, ctx->rledec_len, ctx->rle_intermediate);

    // RLE decode rledec into decomp
    rle_decode(ctx->rledec, ctx->decomp, ctx->rledec_len, &ctx->decomp_len);
//    printf("after rle decode: decomp_len %ld\n", ctx->decomp_len);
    return CARITH_ERR_NONE;
}
