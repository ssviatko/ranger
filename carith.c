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

static void freq_count(carith_comp_ctx *ctx)
{
    size_t i;
    uint64_t base_tab = 0;

    for (i = 0; i < 256; ++i) {
        ctx->freq[i].count_base = 0;
        ctx->freq[i].count = 0;
    }
    for (i = 0; i < ctx->plain_len; ++i) {
        ctx->freq[ctx->plain[i]].count++;
    }
    for (i = 0; i < 256; ++i) {
        ctx->freq[i].count_base = base_tab;
        base_tab += ctx->freq[i].count;
    }
}

static void retrieve_range(carith_comp_ctx *ctx, uint8_t a_token, uint64_t *a_start, uint64_t *a_end)
{
    uint64_t l_rangesize = *a_end - *a_start;
    uint64_t l_start_orig = *a_start;

    //	printf("rr: passed %016lX %016lX ", *a_start, *a_end);
    *a_start = (__uint128_t)l_start_orig + ((__uint128_t)ctx->freq[a_token].count_base * (__uint128_t)l_rangesize) / (__uint128_t)ctx->plain_len;
    *a_end = (__uint128_t)l_start_orig + ((((__uint128_t)ctx->freq[a_token].count_base + (__uint128_t)ctx->freq[a_token].count) * (__uint128_t)l_rangesize) / (__uint128_t)ctx->plain_len) - 1;
    //	printf("assigned token %02X - %ld/%ld/%ld  %016lX %016lX\n", a_token, ctx->freq[a_token].count_base, ctx->freq[a_token].count, ctx->plain_len, *a_start, *a_end);
}

static uint8_t token_for_window(carith_comp_ctx *ctx, uint64_t a_window, uint64_t a_start, uint64_t a_end)
{
    size_t i;
    uint64_t l_rangesize = a_end - a_start;
    uint64_t l_windowpos = a_window - a_start;
    uint64_t l_countpos = ((__uint128_t)l_windowpos * (__uint128_t)ctx->plain_len) / (__uint128_t)l_rangesize;
    //	printf("l_rangesize %016lX l_windowpos %016lX l_countpos %ld plain_len %ld   ", l_rangesize, l_windowpos, l_countpos, ctx->plain_len);
    for (i = 0; i < 256; ++i) {
        //			printf("i %ld %ld %ld\n", i, ctx->freq[i].count_base, ctx->freq[i].count_base + ctx->freq[i].count);
        if ((l_countpos >= ctx->freq[i].count_base) && (l_countpos < ctx->freq[i].count_base + ctx->freq[i].count))
            break;
    }
    // check it to make sure, due to inaccuracies
    uint64_t l_start = a_start;
    uint64_t l_end = a_end;
    retrieve_range(ctx, i, &l_start, &l_end);
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
        retrieve_range(ctx, j, &l_start, &l_end);
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
        free(ctx);
        return CARITH_ERR_MEMORY;
    }
    ctx->comp = NULL;
    ctx->comp = malloc(a_worksize * 3 / 2); // comp guard size 150%
    if (ctx->comp == NULL) {
        free(ctx->plain);
        return CARITH_ERR_MEMORY;
    }
    ctx->decomp = NULL;
    ctx->decomp = malloc(a_worksize);
    if (ctx->decomp == NULL) {
        free(ctx->plain);
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
    free(ctx->comp);
    free(ctx->decomp);
    return CARITH_ERR_NONE;
}

/**
 * @brief Compress plain buffer into comp buffer
 */

carith_error_t carith_compress(carith_comp_ctx *ctx)
{
    size_t plain_ptr;
    uint64_t range_lo, range_hi;
    uint8_t cur_byte;
    uint8_t range_lo_hibyte, range_hi_hibyte; // bits 56-63 of the range
    size_t comp_ptr = 0;
    size_t i;

    freq_count(ctx);

    range_lo = 0;
    range_hi = ULLONG_MAX;

    for (plain_ptr = 0; plain_ptr < ctx->plain_len; ++plain_ptr) {
        cur_byte = ctx->plain[plain_ptr];
        //		range_lo = ctx->freq[cur_byte].range_start;
        //		range_hi = ctx->freq[cur_byte].range_end;
        retrieve_range(ctx, cur_byte, &range_lo, &range_hi);
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
    uint64_t total_comp_len = ctx->freq_comp_len + ctx->comp_len;
    return CARITH_ERR_NONE;
}

/**
 * @brief Decompress comp buffer into decomp buffer
 */

carith_error_t carith_decompress(carith_comp_ctx *ctx)
{

}
