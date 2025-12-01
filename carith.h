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

#ifndef CARITH_H
#define CARITH_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(1)

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "cbit.h"
#include "rle.h"

typedef struct {
    uint64_t            count_base;       ///< Running tally of counts so far in the table
    uint64_t            count;            ///< Number of times this symbol occurs in plaintext
} carith_freq_entry_t;

typedef struct {
    uint8_t             scheme;           ///< compression chain specifier
    uint32_t            block_num;        ///< Optional tag for block number, used by implementation
    carith_freq_entry_t freq[256];        ///< Frequency table, contains list of ranges for all possible symbols
    uint8_t             freq_comp[1024];  ///< Compressed frequency table, either enumerated or full
    cbit_cursor_t       bc;               ///< Bit cursor used by carith to write out frequency tables
    uint16_t            freq_comp_len;    ///< Length of compressed frequency table
    uint8_t            *plain;            ///< Buffer for plaintext to be compressed
    size_t              plain_len;        ///< Plaintext length
    uint8_t            *rleenc;           ///< Buffer for RLE encoded data
    size_t              rle_intermediate; ///< Size of plain smooshed to RLE, if specified in scheme
    uint8_t            *rledec;           ///< Buffer for RLE decoded data
    size_t              rledec_len;       ///< Length of RLE decoded data
    uint8_t             *comp;            ///< Buffer for compressed tokens, larger than plaintext buffer to guard against over-ratio compresions
    size_t              comp_len;         ///< Length of compressed token stream
    uint8_t            *decomp;           ///< Buffer for decompressed data
    size_t              decomp_len;       ///< Length of decompressed data
} carith_comp_ctx;

// scheme bits - order of operations: RLE, then LZW, then AC. OR these together to make a compression chain
const static uint8_t scheme_ac = 0x80;
const static uint8_t scheme_rle = 0x40;
const static uint8_t scheme_lzw = 0x20;

/**
 * @enum carith_error_t
 * @brief An enumerated list of return error codes.
 */

typedef enum {
    CARITH_ERR_NONE,
    CARITH_ERR_MEMORY
} carith_error_t;

const char    *carith_strerror   (carith_error_t a_errno);
carith_error_t carith_init_ctx   (carith_comp_ctx *ctx, size_t a_worksize);
carith_error_t carith_free_ctx   (carith_comp_ctx *ctx);
carith_error_t carith_compress   (carith_comp_ctx *ctx);
carith_error_t carith_extract    (carith_comp_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif // CARITH_H
