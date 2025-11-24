/**
 *
 * C Bit Read/Write Library
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
 * @file cbit.h
 * @brief C Bit Read/Write API
 *
 * Enables the reading/writing of individual bits or sequences of bits to a
 * buffer.
 *
 */

#ifndef CBIT_H
#define CBIT_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(1)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/**
 * @struct cbit_cursor_t
 * @brief The bit cbit_cursor
 *
 * How to use: Initialize this structure with byte = 0, bit = 7, and buffer
 * set to point to a byte buffer that you wish to operate upon.
 */

typedef struct {
    uint64_t byte;   ///< Byte counter into buffer
    uint8_t  bit;    ///< Next bit to manipulate on above byte
    uint8_t *buffer; ///< Pointer to buffer we are working with
} cbit_cursor_t;

void     cbit_write         (cbit_cursor_t *a_cursor, unsigned int a_bit);
void     cbit_write_many    (cbit_cursor_t *a_cursor, uint64_t a_bits, uint16_t a_count);
int      cbit_read          (cbit_cursor_t *a_cursor);
uint64_t cbit_read_many     (cbit_cursor_t *a_cursor, uint16_t a_count);
uint16_t cbit_bit_width     (uint64_t a_val);

#ifdef __cplusplus
}
#endif

#endif // CBIT_H
