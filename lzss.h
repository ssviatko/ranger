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
 * @file lzss.h
 * @brief Lempel/Ziv/Storer/Szymanski dictionary compressor
 *
 * LZSS API
 *
 */

#ifndef LZSS_H
#define LZSS_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(1)

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h> // for htons/htonl

/**
 * @struct token_block_t
 * @brief Block of 8 tokens awaiting write to output stream
 *
 * Used by the encoder, here is where it stores tokens on their way to output
 * in order to write out all 8 flags in one single byte rather than writing
 * out single bits. Using whole bytes saves a lot of time over using the cbit
 * library to output individual bits.
 */

typedef struct {
    int numflags; ///< Number of flags contained in the flags byte (below)
    uint8_t flags; ///< Flags for tokens: 0 = byte token, 1 = match token
    uint16_t tokens[8]; ///< 8 tokens to write to compressed stream
} token_block_t; ///< Token storage block used by the encoder

/**
 * @struct symbol_hint_t
 * @brief Definition of a symbol hint
 *
 * An array of 256 of these is kept in the context, it is used to tabulate the
 * counts of every symbol in the buffer (including the dictionary itself),
 * along with a pointer to start locations in the pointer_pool (also contained in the
 * context) which contains pointers to locations in the buffer where the
 * symbol occurs. Think of count_base/search_base as "pointers to pointers" to
 * hopefully eliminate confusion.
 *
 * count_base contains the starting base location in the pointer pool to start
 * looking for a symbol. As the window slides forward, some "early" symbols in
 * the pointer pool will become unnecessary as we have already passed them.
 * So, the next time we search for the same symbol, we don't want to trip over
 * these obsolete references, so search_base is used instead. count_base and
 * search_base are both initialized to the same value, except when we
 * encounter an obsolete symbol, search_base is incremented, count is decre-
 * mented, and the next time through the loop we will start at the next
 * pointer after the obsolete one. This continues on and on as we slide the
 * window through the buffer until the end.
 */

typedef struct {
    uint32_t count_base; ///< Beginning of symbol's spread in pointer pool
    uint32_t search_base; ///< This pointer moves forward as we slide the window
    uint32_t count; ///< Total number of occurrances of this symbol
    uint32_t next_pool_loc; ///< starts at zero, counts up to count - 1, points to next location in pointer_pool to write pointer (count_base + next_pool_loc)
} symbol_hint_t; ///< Symbol Hint Record

/**
 * @struct lzss_comp_ctx
 * @brief The LZSS context
 *
 * This is the block of contextual data that the LZSS system uses to address
 * a specific LZSS session.
 */

typedef struct {
    uint32_t *pointer_pool; ///< pointers, one for every byte in the buffer, which is typically [WINDOW_SIZE + SEGSIZE]
    symbol_hint_t symbols[256]; ///< Array of symbol hints
    uint32_t seed_dictionary_start; ///< Pointer to start of seeded dictionary. This is where we open the window at the start of encoding.
} lzss_comp_ctx; ///< LZSS Compression Context

/**
 * @enum lzss_error_t
 * @brief An enumerated list of return error codes.
 */

typedef enum {
    LZSS_ERR_NONE,
    LZSS_ERR_MEMORY,
    LZSS_ERR_ZEROIN
} lzss_error_t;

const char     *lzss_strerror                   (lzss_error_t a_errno);
lzss_error_t    lzss_prepare_dictionary         (lzss_comp_ctx *ctx, const uint8_t *a_seed, size_t a_seed_len, uint8_t *a_buffer);
lzss_error_t    lzss_prepare_default_dictionary (lzss_comp_ctx *ctx, uint8_t *a_buffer);
lzss_error_t    lzss_init_context               (lzss_comp_ctx *ctx, size_t a_worksize);
lzss_error_t    lzss_free_context               (lzss_comp_ctx *ctx);
lzss_error_t    lzss_prepare_pointer_pool       (lzss_comp_ctx *ctx, uint8_t *a_in, size_t a_in_len);
lzss_error_t    lzss_encode                     (lzss_comp_ctx *ctx, uint8_t *a_in, size_t a_in_len, uint8_t *a_out, size_t *a_out_len);
lzss_error_t    lzss_decode                     (lzss_comp_ctx *ctx, uint8_t *a_in, size_t a_in_len, uint8_t *a_out, size_t *a_out_len);

#ifdef __cplusplus
}
#endif

#endif // LZSS_H
