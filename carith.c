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
 * @file carith.c
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
    // LZSS stuff
    ctx->lzssenc = NULL;
    ctx->lzssenc = malloc(LZSS_WINDOW_SIZE + a_worksize);
    if (ctx->lzssenc == NULL) {
        free(ctx->plain);
        free(ctx->rleenc);
        free(ctx->rledec);
        free(ctx->comp);
        free(ctx->decomp);
        return CARITH_ERR_MEMORY;
    }
    ctx->lzssdec = NULL;
    ctx->lzssdec = malloc(LZSS_WINDOW_SIZE + a_worksize);
    if (ctx->lzssdec == NULL) {
        free(ctx->plain);
        free(ctx->rleenc);
        free(ctx->rledec);
        free(ctx->comp);
        free(ctx->decomp);
        free(ctx->lzssenc);
        return CARITH_ERR_MEMORY;
    }

    // init our LZSS context
    lzss_error_t err;
    err = lzss_init_context(&ctx->lzss_context, a_worksize);
    if (err != LZSS_ERR_NONE) {
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
    lzss_free_context(&ctx->lzss_context);
    free(ctx->plain);
    free(ctx->rleenc);
    free(ctx->rledec);
    free(ctx->comp);
    free(ctx->decomp);
    free(ctx->lzssenc);
    free(ctx->lzssdec);
    return CARITH_ERR_NONE;
}

/**
 * @brief Compress plain buffer into comp buffer
 */

// void ccct_print_hex(uint8_t *a_buffer, size_t a_len)
// {
//     unsigned int g_col = 180;
//     unsigned int i;
//     unsigned int l_bytes_to_print = (g_col / 48) * 16;
//     for (i = 0; i < a_len; ++i) {
//         if (i % l_bytes_to_print == 0)
//             printf("\n");
//         printf("%02X ", a_buffer[i]);
//     }
//     printf("\n");
// }

void ccct_print_hex(uint8_t *a_buffer, size_t a_len)
{
    unsigned int i;
    unsigned int l_bytes_to_print = (200 / 48) * 16;
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

    ctx->rle_intermediate = 0; // for AC only operation, will be changed if RLE is on
    ctx->lzss_intermediate = 0;

//    // sanity check
//    uint8_t sanity[1048576];
//    size_t sanity_count;

// five options here: RLE only, RLE/LZSS/AC, RLE/AC, LZSS/AC, and AC only.
    enum { RLEONLY, LZSSONLY, RLELZSSAC, RLEAC, LZSSAC, ACONLY } l_schemenum;
    switch (ctx->scheme & 0xe0) {
        case 0x40: l_schemenum = RLEONLY; break;
        case 0x20: l_schemenum = LZSSONLY; break;
        case 0x80: l_schemenum = ACONLY; break;
        case 0xc0: l_schemenum = RLEAC; break;
        case 0xe0: l_schemenum = RLELZSSAC; break;
        case 0xa0: l_schemenum = LZSSAC; break;
        default: {
            fprintf(stderr, "unexpected scheme byte value: %02X\n", ctx->scheme);
            exit(EXIT_FAILURE);
        }
    }

    lzss_error_t err;

    if (l_schemenum == RLEONLY) {
        rle_encode(ctx->plain, ctx->comp, ctx->plain_len, &ctx->rle_intermediate);
        ctx->freq_comp_len = 0;
        ctx->comp_len = ctx->rle_intermediate;
        return CARITH_ERR_NONE;
    } else if (l_schemenum == RLEAC) {
        rle_encode(ctx->plain, ctx->rleenc, ctx->plain_len, &ctx->rle_intermediate);
        ac_source = ctx->rleenc;
        ac_source_size = ctx->rle_intermediate;
    } else if (l_schemenum == LZSSONLY) {
//        printf("ctx->plain (%ld) ", ctx->plain_len);
//        ccct_print_hex(ctx->plain, ctx->plain_len);
        // moe plain into rleenc and make space for window
        memcpy(ctx->rleenc + LZSS_WINDOW_SIZE, ctx->plain, ctx->plain_len);
//        printf("rleenc window at %d (%ld) ", LZSS_WINDOW_SIZE, ctx->plain_len);
//        ccct_print_hex(ctx->rleenc + LZSS_WINDOW_SIZE, ctx->plain_len);
        lzss_prepare_default_dictionary(&ctx->lzss_context, ctx->rleenc);
//        printf("rleenc dictionary: start at %d ", ctx->lzss_context.seed_dictionary_start);
//        ccct_print_hex(ctx->rleenc + ctx->lzss_context.seed_dictionary_start, LZSS_WINDOW_SIZE - ctx->lzss_context.seed_dictionary_start);
        lzss_prepare_pointer_pool(&ctx->lzss_context, ctx->rleenc, ctx->plain_len);
        //encode lzss from rleenc -> comp
        err = lzss_encode(&ctx->lzss_context, ctx->rleenc, ctx->plain_len, ctx->comp, &ctx->lzss_intermediate);
        if (err != LZSS_ERR_NONE) {
            fprintf(stderr, "lzss error: %s", lzss_strerror(err));
            exit(EXIT_FAILURE);
        }
//        printf("comp (%ld) ", ctx->lzss_intermediate);
//        ccct_print_hex(ctx->comp, ctx->lzss_intermediate);
        ctx->freq_comp_len = 0;
        ctx->comp_len = ctx->lzss_intermediate;
        return CARITH_ERR_NONE;
    } else if (l_schemenum == RLELZSSAC) {
        rle_encode(ctx->plain, ctx->rleenc + LZSS_WINDOW_SIZE, ctx->plain_len, &ctx->rle_intermediate);
        lzss_prepare_default_dictionary(&ctx->lzss_context, ctx->rleenc);
        lzss_prepare_pointer_pool(&ctx->lzss_context, ctx->rleenc, ctx->rle_intermediate);
        err = lzss_encode(&ctx->lzss_context, ctx->rleenc, ctx->rle_intermediate, ctx->lzssenc, &ctx->lzss_intermediate);
        if (err != LZSS_ERR_NONE) {
            fprintf(stderr, "lzss error: %s", lzss_strerror(err));
            exit(EXIT_FAILURE);
        }
        ac_source = ctx->lzssenc;
        ac_source_size = ctx->lzss_intermediate;
    } else if (l_schemenum == LZSSAC) {
        // move plain into rleenc and make space for window
        memcpy(ctx->rleenc + LZSS_WINDOW_SIZE, ctx->plain, ctx->plain_len);
        lzss_prepare_default_dictionary(&ctx->lzss_context, ctx->rleenc);
        lzss_prepare_pointer_pool(&ctx->lzss_context, ctx->rleenc, ctx->plain_len);
        err = lzss_encode(&ctx->lzss_context, ctx->rleenc, ctx->plain_len, ctx->lzssenc, &ctx->lzss_intermediate);
        if (err != LZSS_ERR_NONE) {
            fprintf(stderr, "lzss error: %s", lzss_strerror(err));
            exit(EXIT_FAILURE);
        }
//        printf("lzssenc (%ld) ", ctx->lzss_intermediate);
//        ccct_print_hex(ctx->lzssenc, ctx->lzss_intermediate);
        ac_source = ctx->lzssenc;
        ac_source_size = ctx->lzss_intermediate;
    } else if (l_schemenum == ACONLY) {
        // we're already set up with the AC source set to plain, so do nothing...
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

//    printf("AC output (%ld) ", ctx->comp_len);
//    ccct_print_hex(ctx->comp, ctx->comp_len);

    return CARITH_ERR_NONE;
}

/**
 * @brief Decompress comp buffer into decomp buffer
 */

carith_error_t carith_extract(carith_comp_ctx *ctx)
{
    uint8_t *ac_dest = ctx->decomp;
    size_t ac_source_size = ctx->plain_len;
    // set these up to default to ACONLY configuration
    size_t *ac_dest_size;
    ac_dest_size = &ctx->decomp_len;

//    printf("input (%ld) ", ctx->comp_len);
//    ccct_print_hex(ctx->comp, ctx->comp_len);

    // five options here: RLE only, RLE/LZSS/AC, RLE/AC, LZSS/AC, and AC only.
    enum { RLEONLY, LZSSONLY, RLELZSSAC, RLEAC, LZSSAC, ACONLY } l_schemenum;
    switch (ctx->scheme & 0xe0) {
        case 0x40: l_schemenum = RLEONLY; break;
        case 0x20: l_schemenum = LZSSONLY; break;
        case 0x80: l_schemenum = ACONLY; break;
        case 0xc0: l_schemenum = RLEAC; break;
        case 0xe0: l_schemenum = RLELZSSAC; break;
        case 0xa0: l_schemenum = LZSSAC; break;
        default: {
            fprintf(stderr, "unexpected scheme byte value: %02X\n", ctx->scheme);
            exit(EXIT_FAILURE);
        }
    }

    // if we're doing RLE or LZSS only, just skip all the AC stuff
    if ((l_schemenum == RLEONLY) || (l_schemenum == LZSSONLY))
        goto carith_extract_skipac;

    // if we're using AC and RLE, change ac_dest to point to rledec
    if (l_schemenum == RLEAC) {
        ac_dest = ctx->rledec;
        ac_dest_size = &ctx->rledec_len;
        ac_source_size = ctx->rle_intermediate;
    } else if ((l_schemenum == RLELZSSAC) || (l_schemenum == LZSSAC)) {
        // decompressed AC stream goes to lzssdec
        ac_dest = ctx->lzssdec;
        ac_dest_size = &ctx->lzssdec_len;
        ac_source_size = ctx->lzss_intermediate;
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
    if (l_schemenum == ACONLY)
        return CARITH_ERR_NONE;

carith_extract_skipac:

    lzss_error_t err;

    // if we did rle only, copy comp into rledec
    if (l_schemenum == RLEONLY) {
        memcpy(ctx->rledec, ctx->comp, ctx->comp_len);
        ctx->rledec_len = ctx->comp_len;
        // RLE decode rledec into decomp
        rle_decode(ctx->rledec, ctx->decomp, ctx->rledec_len, &ctx->decomp_len);
    } else if (l_schemenum == LZSSONLY) {
//        printf("ctx->comp (%ld) ", ctx->comp_len);
//        ccct_print_hex(ctx->comp, ctx->comp_len);
        err = lzss_prepare_default_dictionary(&ctx->lzss_context, ctx->rledec);
        err = lzss_decode(&ctx->lzss_context, ctx->comp, ctx->comp_len, ctx->rledec, &ctx->rledec_len);
        if (err != LZSS_ERR_NONE) {
            fprintf(stderr, "lzss error: %s", lzss_strerror(err));
            exit(EXIT_FAILURE);
        }
//        printf("ctx->rledec + window(%ld) ", ctx->rledec_len);
//        ccct_print_hex(ctx->rledec + LZSS_WINDOW_SIZE, ctx->rledec_len);
        memcpy(ctx->decomp, ctx->rledec + LZSS_WINDOW_SIZE, ctx->rledec_len);
        ctx->decomp_len = ctx->rledec_len;
//        printf("ctx->decomp (%ld) ", ctx->decomp_len);
//        ccct_print_hex(ctx->decomp, ctx->decomp_len);
    } else if (l_schemenum == RLEAC) {
        // AC operation decomped into rledec, so decode it
        rle_decode(ctx->rledec, ctx->decomp, ctx->rledec_len, &ctx->decomp_len);
    } else if (l_schemenum == RLELZSSAC) {
        // decompress LZSS tokens waiting in lzssdec into rledec.
        // remember, rledec will be windowed after this operation.
        lzss_prepare_default_dictionary(&ctx->lzss_context, ctx->rledec);
        err = lzss_decode(&ctx->lzss_context, ctx->lzssdec, ctx->lzssdec_len, ctx->rledec, &ctx->rledec_len);
        if (err != LZSS_ERR_NONE) {
            fprintf(stderr, "lzss error: %s", lzss_strerror(err));
            exit(EXIT_FAILURE);
        }
        // and then do the RLE decode
        rle_decode(ctx->rledec + LZSS_WINDOW_SIZE, ctx->decomp, ctx->rledec_len, &ctx->decomp_len);
    } else if (l_schemenum == LZSSAC) {
        // decompress into lzssdec, then copy the text after the window into decomp and set decomp's len
        lzss_prepare_default_dictionary(&ctx->lzss_context, ctx->rledec);
        err = lzss_decode(&ctx->lzss_context, ctx->lzssdec, ctx->lzssdec_len, ctx->rledec, &ctx->rledec_len);
        if (err != LZSS_ERR_NONE) {
            fprintf(stderr, "lzss error: %s what the AC decompressor gave us: len %ld", lzss_strerror(err), ctx->lzssdec_len);
            ccct_print_hex(ctx->lzssdec, ctx->lzssdec_len);
            exit(EXIT_FAILURE);
        }
//        printf("lzss output: (%ld) ", ctx->rledec_len);
//        ccct_print_hex(ctx->rledec + LZSS_WINDOW_SIZE, ctx->rledec_len);
        memcpy(ctx->decomp, ctx->rledec + LZSS_WINDOW_SIZE, ctx->rledec_len);
        ctx->decomp_len = ctx->rledec_len;
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

//    printf("after rle decode: decomp_len %ld\n", ctx->decomp_len);
    return CARITH_ERR_NONE;
}
