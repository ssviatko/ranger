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
 * @file rle.h
 * @brief Run Length Encoder API
 *
 * Compresses data by removing runs of repeated characters.
 *
 */

#ifndef RLE_H
#define RLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

void rle_encode(uint8_t *a_in, uint8_t *a_out, size_t a_insize, size_t *a_outsize);
void rle_decode(uint8_t *a_in, uint8_t *a_out, size_t a_insize, size_t *a_outsize);

#ifdef __cplusplus
}
#endif

#endif // RLE_H
