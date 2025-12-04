/**
 *
 * LZSS API
 * 2025/Dec/03 - Revision 0.80 alpha
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
 * @file lzss.c
 * @brief Lempel/Ziv/Storer/Szymanski dictionary compressor
 *
 * LZSS API
 *
 */

#include "lzss.h"

static uint32_t WINDOW_SIZE = 4095; ///< Size of sliding window containing previously seen tokens. Optionally seeded with a pre-defined dictionary.
static uint8_t MINMATCH = 3; ///< Minimum match length of a match token
static uint8_t MAXMATCH = 18; ///< Maximum match length of a match token

static uint32_t OFFSET_INITIAL_COPY = 0; ///< Position in the output buffer where the encoder puts the initial copy length in network byte order.
static uint32_t OFFSET_TOKEN_COUNT = 4; ///< Position in the output buffer where the encoder puts the token count in network byte order.
static uint32_t OFFSET_OUTPUT_STREAM = 8; ///< Position in the output buffer where the encoder begins writing the output byte/token stream.

static const char *default_seed = "the and over if else printf do while goto define include size_t int unsigned uint8_t uint16_t uint32_t uint64_t for void return char short long long static typedef union enum stdio.h stdlib.h errno.h string.h iostream map queue list stack sys/fcntl.h sys/time.h unistd.h class public private protected default memcpy memset volatile pthread exit mutex condition";

const char *lzss_error_string[] = {
    "none",
    "memory allocation error",
    "zero length input"
}; ///< List of standard LZSS error strings correlated to integer LZSS error codes.

/**
 * @brief Returns a char pointer to an existing error string
 * Works in exactly the same way as the strerror(errno) function works in the standard library
 *
 * @param[in] a_errno The numerical error returned by the function
 * @return character pointer to error message
 */

const char *lzss_strerror(lzss_error_t a_errno)
{
    return lzss_error_string[a_errno];
}

/**
 * @brief Install custom seed dictionary into buffer
 *
 * At the start of encoding, a typical LZSS system will have no window to call
 * upon to find matches. It has to slowly build a window by sliding forward,
 * and this can cause poor compression ratios in smaller files (less than 4095
 * bytes in length).
 *
 * My solution to this problem is to load up a custom "seed dictionary" to fill
 * the space to the left of the window pointer while the window is creeping
 * forward between 0 and (typically 4095) bytes. This will give the encoder
 * some default values to reference, and hopefully find matches quicker.
 */

lzss_error_t lzss_prepare_dictionary(lzss_comp_ctx *ctx, const uint8_t *a_seed, size_t a_seed_len, uint8_t *a_buffer)
{
    // zero the dictionary space
    memset(a_buffer, 0, WINDOW_SIZE);
    // copy in seed dictionary, slamming it up as close to the window start as possible
    memcpy(a_buffer + WINDOW_SIZE - a_seed_len, a_seed, a_seed_len);
    // set start pointer to first byte of seed dictionary we just copied
    ctx->seed_dictionary_start = WINDOW_SIZE - a_seed_len;
    return LZSS_ERR_NONE;
}

/**
 * @brief Install the default seed dictionary into buffer
 *
 * The default dictionary above contains the most common English words,
 * words used in programming and other technical disciplines, and commonly
 * repeated bytes.
 */

lzss_error_t lzss_prepare_default_dictionary(lzss_comp_ctx *ctx, uint8_t *a_buffer)
{
    size_t l_default_seed_len = strlen(default_seed);
    return lzss_prepare_dictionary(ctx, (const uint8_t *)default_seed, l_default_seed_len, a_buffer);
}

/**
 * @brief Initialize a LZSS context
 *
 * Must be called before any other operations are attempted. This function
 * allocates space for the internal buffers in the LZSS context.
 *
 * @param[in] ctx Pointer to a LZSS context object
 * @param[in] a_worksize Size in bytes of requested compression segment
 */

lzss_error_t lzss_init_context(lzss_comp_ctx *ctx, size_t a_worksize)
{
    ctx->pointer_pool = NULL;
    ctx->pointer_pool = malloc((a_worksize * sizeof(uint32_t) + (WINDOW_SIZE * sizeof(uint32_t)))); // enough for the window and the input data
    if (ctx->pointer_pool == NULL) {
        return LZSS_ERR_MEMORY;
    }
    return LZSS_ERR_NONE;
}

/**
 * @brief Free a LZSS context
 * Release all allocated memory.
 */

lzss_error_t lzss_free_context(lzss_comp_ctx *ctx)
{
    free(ctx->pointer_pool);
    return LZSS_ERR_NONE;
}

/**
 * @brief Count symbols and build pointer pool
 *
 * Call this after calling lzss_prepare_dictionary
 */

lzss_error_t lzss_prepare_pointer_pool(lzss_comp_ctx *ctx, uint8_t *a_in, size_t a_in_len)
{
    uint32_t i;
    uint32_t base_tab = 0;

    // zero the symbols table
    for (i = 0; i < 256; ++i) {
        ctx->symbols[i].count_base = 0;
        ctx->symbols[i].search_base = 0;
        ctx->symbols[i].count = 0;
        ctx->symbols[i].next_pool_loc = 0;
    }
    // update counts for every symbol
    for (i = ctx->seed_dictionary_start; i < WINDOW_SIZE + a_in_len; i++) {
        ctx->symbols[a_in[i]].count++;
    }
    // set count_base values
    for (i = 0; i < 256; ++i) {
        ctx->symbols[i].count_base = base_tab;
        ctx->symbols[i].search_base = base_tab;
        base_tab += ctx->symbols[i].count;
    }
    // establish pointers
    for (i = ctx->seed_dictionary_start; i < WINDOW_SIZE + a_in_len; i++) {
        ctx->pointer_pool[  ctx->symbols[a_in[i]].count_base + ctx->symbols[a_in[i]].next_pool_loc++  ] = i;
    }
    return LZSS_ERR_NONE;
}

/**
 * @brief Match routine, helper for lzss_encode
 */

static void lzss_match(lzss_comp_ctx *ctx, uint8_t *a_in, uint32_t a_window_back, uint32_t a_window_ptr, uint32_t a_window_ptr_limit, uint16_t *a_match_back_ptr, uint8_t *a_match_len)
{
    // sanity check a_window_back
    if (a_window_back > a_window_ptr - MINMATCH) {
        //        printf("lzss_match: no dictionary or dictionary too small\n");
        return; // window_back nonexistant or less than MINMATCH (will only happen if we start without a seed dictionary)
    }

    // crawl backwards in attempt to find largest match
    *a_match_back_ptr = 0;
    *a_match_len = 0;
    uint8_t biggest_match = 0;
    uint16_t biggest_match_ptr = 0;
    uint8_t current_match = 0;
    uint8_t sym = a_in[a_window_ptr]; // the iniial symbol we are searching for
    uint32_t sym_search_base = ctx->symbols[sym].search_base;
    uint32_t sym_count = ctx->symbols[sym].count;
    for (uint32_t offset = 0; offset < sym_count; ++offset) {
        uint32_t sym_location = ctx->pointer_pool[sym_search_base + offset];
        //        printf("lzss_match: a_window_ptr %d sym %02X pointer_pool entry %d\n", a_window_ptr, sym, sym_location);
        // is sym_location behind the current window_back?
        if (sym_location < a_window_back) {
            // yes, so delete it from the count
            ctx->symbols[sym].count--;
            ctx->symbols[sym].search_base++;
            //            printf("lzss_match: omitting location from count\n");
            continue; // recycle and search next location in pointer pool
        }
        // is sym_location past window_ptr - MINMATCH?
        if (sym_location > a_window_ptr - MINMATCH) {
            //            printf("lzss_match: pointer is after window_ptr - MINMATCH, breaking\n");
            break;
        }
        // got a pointer that is inside the window, so establish target size
        size_t target = a_window_ptr + MAXMATCH; // one after the last byte to target
        if (target >= a_window_ptr_limit)
            target = a_window_ptr_limit; // adjust target if it falls off the end of in buffer
        uint32_t max_in_window = a_window_ptr - sym_location;
        //            printf("lzss_match: max_in_window %d window_ptr_delta %d theoretical target %d match_ptr %d target %d\n", max_in_window, window_ptr_delta, (window_ptr_delta + max_in_window), match_ptr, target);
        if ((a_window_ptr + max_in_window) < target)
            target = a_window_ptr + max_in_window; // if we're not far back enough in the dictionary to match a while MAXMATCH string...
            //        printf("lzss_match: sym_location %d! searching %d to %ld...\n", sym_location, a_window_ptr, target);

        uint32_t compare_ptr;
        for (current_match = 0, compare_ptr = a_window_ptr; compare_ptr < target; ++current_match, ++compare_ptr) {
            if (a_in[compare_ptr] != a_in[sym_location + current_match])
                break;
        }
        // upon break, current_match will hold number of characters matched from match_ptr to ...
        if (current_match > biggest_match) {
            biggest_match = current_match;
            biggest_match_ptr = sym_location;
        }
        //        printf("lzss_match: biggest match so far at %d len %d\n", biggest_match_ptr, biggest_match);
        // largest match 18?
        if (biggest_match == MAXMATCH)
            break;
    }
    *a_match_len = biggest_match;
    *a_match_back_ptr = a_window_ptr - biggest_match_ptr;
}

/**
 * @brief Flush token block to output stream, helper for lzss_encode
 */

static void flush_tb(token_block_t *a_tb, uint8_t *a_out, size_t *a_out_pos)
{
    size_t i;

    if (a_tb->numflags < 8)
        a_tb->flags >>= (8 - a_tb->numflags); // scoot them over so flag #0 starts at bit position 0

//    printf("flush_tb: flags %02X a_out_pos %ld tokens %d\n", a_tb->flags, *a_out_pos, a_tb->numflags);
    a_out[WINDOW_SIZE + (*a_out_pos)++] = a_tb->flags;
    for (i = 0; i < a_tb->numflags; ++i) {
        if ((a_tb->flags & 0x01) == 0x01) {
            // write match token
//            printf("write match token incr %ld\n", *a_out_pos);
            uint16_t l_temp16 = htons(a_tb->tokens[i]);
            memcpy(a_out + *a_out_pos + WINDOW_SIZE, &l_temp16, sizeof(l_temp16));
            *a_out_pos += 2;
        } else {
            // write byte token
//            printf("write byte token incr %ld\n", *a_out_pos);
            uint8_t l_temp8 = a_tb->tokens[i];
            memcpy(a_out + *a_out_pos + WINDOW_SIZE, &l_temp8, sizeof(l_temp8));
            (*a_out_pos)++;
        }
        a_tb->flags >>= 1;
    }
//    printf("incr %ld\n", *a_out_pos);
}

/**
 * @brief Encode an LZSS block
 *
 * This is the main compression routine. It takes a buffer as input which is
 * large enough to hold a complete window (typically 4095 bytes) plus all of
 * the input data. For example, if the user is compressing 64k of data, then
 * the input buffer must be capable of holding 68k (the 4k window + the 64k).
 *
 * The window should be seeded with a pre-defined dictionary. If you have no
 * dictionary prepared, then you should call lzss_prepare_default_dictionary
 * before doing the symbol count with lzss_prepare_pointer_pool. Having a
 * pre-defined seed dictionary radically improves the compression ratio on
 * small files of 4k or less. If you choose to make your own custom dictionary,
 * it should contain words/phrases and/or byte sequences that you imagine to
 * be common to the data you intend to encode.
 *
 * The output buffer must be large enough to contain all of the compression
 * tokens, plus the 12 bytes of informational data the compressor writes at the
 * start of the output stream. Note that an uncompressible input stream will
 * result in writing excessive byte tokens with very few match tokens, and
 * with a 9/8 ratio of output data to input bytes this will result in a
 * compression ratio of around 112.5%. For this reason the output buffer should
 * be at least 9/8ths the size of the input data, and should include
 * additional space as a guard on the end of that. A conservtive recommendation
 * would be to make the output buffer 3/2 the size of the input buffer. This
 * is the size used in the lzss_test demonstration program, and it is probably
 * overkill, but it will guarantee that you will never overrun the buffer.
 *
 * @param[in] ctx The LZSS Context
 * @param[in] a_in Pointer to buffer containing window + input data
 * @param[in] a_in_len Length of buffer (window + input data)
 * @param[in] a_out Pointer to buffer large enough to contain compression tokens
 * @param[out] a_out_len The length of the output data
 */

lzss_error_t lzss_encode(lzss_comp_ctx *ctx, uint8_t *a_in, size_t a_in_len, uint8_t *a_out, size_t *a_out_len)
{
    size_t i;
    uint32_t window_ptr = WINDOW_SIZE; // start at 4096
    uint32_t window_ptr_limit = WINDOW_SIZE + a_in_len; // one after the last byte of in buffer
    uint32_t window_back = ctx->seed_dictionary_start;
    uint32_t initial_copy = 0; ///< Number of bytes initially copied directly before first match
    int found_first_match = 0; // set to 1 if we matched something so we can start using 8-token blocks
    uint32_t token_count = 0; ///< Number of tokens we have encoded thus far
    size_t out_ptr = OFFSET_OUTPUT_STREAM; ///< Next position to write in the out buffer
    uint16_t match_back_ptr = 0; ///< Variable to hold match locations
    uint8_t match_len = 0; ///< Variable to hold match lengths, holds value MINMATCH >= value <= MAXMATCH
    token_block_t tb;
    memset(&tb, 0, sizeof(tb));

    // sanity check a_in_len
    if (a_in_len == 0) {
        *a_out_len = 0;
        return LZSS_ERR_ZEROIN;
    }

//    printf("lzss_encode: starting window_back %d window_ptr %d window_ptr_limit %d\n", window_back, window_ptr, window_ptr_limit);
    // match loop
    do {
        // move window_back if we advanced window_ptr past the established window size
        if ((window_ptr - window_back) > WINDOW_SIZE)
            window_back = window_ptr - WINDOW_SIZE;
        lzss_match(ctx, a_in, window_back, window_ptr, window_ptr_limit, &match_back_ptr, &match_len);
//        printf("lzss_encode: window_ptr %d - lzss_match returned %d, match_back_ptr %d match_len %d\n", window_ptr, res, match_back_ptr, match_len);
        if (match_len < MINMATCH) {
            if (match_len == 0) match_len = 1; // no match, still want to write at least 1 byte
            for (i = 0; i < match_len; ++i) {
                if (found_first_match == 0) {
//                    char c = 0x20;
//                    if ((a_in[window_ptr] > 0x1f) && (a_in[window_ptr] < 0x80))
//                        c = a_in[window_ptr];
//                    printf("byte copy  output : %ld - %02X (%c)\n", out_ptr, a_in[window_ptr], c);
                    a_out[WINDOW_SIZE + out_ptr] = a_in[window_ptr];
                    initial_copy++;
                    out_ptr++;
                } else {
//                    char c = 0x20;
//                    if ((a_in[window_ptr] > 0x1f) && (a_in[window_ptr] < 0x80))
//                        c = a_in[window_ptr];
//                    printf("byte token output : %ld - %02X (%c)\n", out_ptr, a_in[window_ptr], c);
                    tb.flags >>= 1;
                    tb.tokens[tb.numflags] = a_in[window_ptr];
                    tb.numflags++;
                    token_count++;
                    if (tb.numflags == 8) {
                        //                        printf("interior to for loop: token block full at token_count %d numflags %d.\n", token_count, tb.numflags);
                        flush_tb(&tb, a_out, &out_ptr);
                        memset(&tb, 0, sizeof(tb));
                    }
                }
                window_ptr++;
            }
        } else {
            if (found_first_match == 0) found_first_match++;
//            char cc[20]; memset(cc, 0, 20);
//            strncpy(cc, (char *)(a_in + window_ptr - match_back_ptr), match_len);
//            for (i = 0; i < strlen(cc); ++i) {
//                if (cc[i] < 0x20) cc[i] = '.';
//                if (cc[i] > 0x7f) cc[i] = '.';
//            }
//            printf("match token output: %ld - <%d, %d> (%s)\n", out_ptr, match_back_ptr, match_len, cc);
            window_ptr += match_len;
            tb.flags >>= 1;
            tb.flags |= 0x80;
            tb.tokens[tb.numflags] = (match_back_ptr << 4) + (match_len - MINMATCH);
            //            printf("match token: %04X\n", tb.tokens[tb.numflags]);
            tb.numflags++;
            token_count++;
        }
        if (tb.numflags == 8) {
//            printf("token block full at token_count %d numflags %d.\n", token_count, tb.numflags);
            flush_tb(&tb, a_out, &out_ptr);
            memset(&tb, 0, sizeof(tb));
        }
    } while (window_ptr < window_ptr_limit);
    // final flush of tb
    if (tb.numflags > 0) {
        flush_tb(&tb, a_out, &out_ptr);
    }
//    printf("lzss_encode: initial_copy %d token_count %d a_in_len %ld out_ptr %ld\n", initial_copy, token_count, a_in_len, out_ptr);
    initial_copy = htonl(initial_copy);
    memcpy(a_out + OFFSET_INITIAL_COPY + WINDOW_SIZE, &initial_copy, sizeof(initial_copy));
    token_count = htonl(token_count);
    memcpy(a_out + OFFSET_TOKEN_COUNT + WINDOW_SIZE, &token_count, sizeof(token_count));
    *a_out_len = out_ptr;
    return LZSS_ERR_NONE;
}

/**
 * @brief Decode an LZSS block
 *
 * This is the main decompression routine. It takes a buffer of compression
 * tokens as input and a buffer large enough to hold the window and the output
 * data. Note that the window must be seeded with the same dictionary that was
 * used to compress the data in the lzss_encode function above.
 *
 * NOTE: Using the wrong dictionary will result in corrupt output data!
 *
 * Same advice for output buffer size in the Encode routine applies here. it
 * is recommended that the output buffer be 3/2 the size of expected
 * decompressed plain text plus the size of the window.
 *
 * @param[in] ctx The LZSS Context
 * @param[in] a_in Pointer to buffer containing compression tokens
 * @param[in] a_in_len Length of buffer containing compression tokens
 * @param[in] a_out Pointer to windowed buffer with appropriate seed dictionary and space for output
 * @param[out] a_out_len The length of the output data
 */

lzss_error_t lzss_decode(lzss_comp_ctx *ctx, uint8_t *a_in, size_t a_in_len, uint8_t *a_out, size_t *a_out_len)
{
    uint32_t initial_copy;
    memcpy(&initial_copy, a_in + OFFSET_INITIAL_COPY + WINDOW_SIZE, sizeof(initial_copy));
    initial_copy = ntohl(initial_copy);
    uint32_t token_count;
    memcpy(&token_count, a_in + OFFSET_TOKEN_COUNT + WINDOW_SIZE, sizeof(token_count));
    token_count = ntohl(token_count);
//    printf("lzss_decode: token_count %d initial_copy %d\n", token_count, initial_copy);

    size_t in_ptr = OFFSET_OUTPUT_STREAM;
    size_t out_ptr = 0;
    size_t i, j;
    *a_out_len = 0;

    // do initial copy of raw bytes
    for (i = 0; i < initial_copy; ++i) {
        a_out[WINDOW_SIZE + out_ptr++] = a_in[WINDOW_SIZE + in_ptr++];
    }
//    printf("lzss_decode: initial copy %d bytes in_ptr %ld out_ptr %ld\n", initial_copy, in_ptr, out_ptr);

    // read in token_count tokens in 8 token increments
    for (i = 0; i < token_count; i += 8) {
        uint8_t flags = a_in[WINDOW_SIZE + in_ptr++];
        //        printf("lzss_decode: in_ptr %ld i %ld flags %02X\n", in_ptr, i, flags);
        for (j = 0; j < 8; ++j) {
            if ((i + j) >= token_count)
                goto lzss_decode_done;
            if ((flags & 0x01) == 0x01) {
                // read match token
                uint16_t l_temp16;
                memcpy(&l_temp16, a_in + WINDOW_SIZE + in_ptr, sizeof(l_temp16));
                l_temp16 = ntohs(l_temp16);
                uint16_t match_back_ptr = l_temp16 >> 4;
                uint8_t match_len = l_temp16 & 0xf;
                match_len += MINMATCH;
                memcpy(a_out + out_ptr + WINDOW_SIZE, a_out + out_ptr + WINDOW_SIZE - match_back_ptr, match_len);
//                // report
//                char cc[20]; memset(cc, 0, 20);
//                strncpy(cc, (char *)(a_out + out_ptr + WINDOW_SIZE - match_back_ptr), match_len);
//                for (size_t ii = 0; ii < strlen(cc); ++ii) {
//                    if (cc[ii] < 0x20) cc[ii] = '.';
//                    if (cc[ii] > 0x7f) cc[ii] = '.';
//                }
//                printf("match token input: i %ld j %ld in_ptr %ld out_ptr %ld - <%d, %d> (%s)\n", i, j, in_ptr, out_ptr, match_back_ptr, match_len, cc);
                in_ptr += 2;
                out_ptr += match_len;
            } else {
                //read byte token
                uint8_t l_temp8;
                memcpy(&l_temp8, a_in + WINDOW_SIZE + in_ptr, sizeof(l_temp8));
                memcpy(a_out + WINDOW_SIZE + out_ptr, &l_temp8, sizeof(l_temp8));
//                // report
//                char c = 0x20;
//                if ((l_temp8 > 0x1f) && (l_temp8 < 0x80))
//                    c = l_temp8;
//                printf("byte copy input: i %ld j %ld in_ptr %ld out_ptr %ld - %02X (%c)\n", i, j, in_ptr, out_ptr, l_temp8, c);
                in_ptr++;
                out_ptr++;
            }
            flags >>= 1;
        }
    }

lzss_decode_done:
    *a_out_len = out_ptr;
    return LZSS_ERR_NONE;
}
