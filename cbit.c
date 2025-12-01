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
 * @file cbit.c
 * @brief C Bit Read/Write API
 *
 * Enables the reading/writing of individual bits or sequences of bits to a
 * buffer.
 *
 */

#include "cbit.h"

static uint8_t cbit_byte_mask[] = { 0xfe, 0xfd, 0xfb, 0xf7, 0xef, 0xdf, 0xbf, 0x7f }; ///< Byte mask to mask off requested bit

/**
 * @brief Write a single bit at current cursor position
 *
 * @param[in] a_cursor Pointr to the cursor to use
 * @param[in] a_bit Integer value 0=false 1=true
 */

void cbit_write(cbit_cursor_t *a_cursor, unsigned int a_bit)
{
	// make masks
	uint8_t l_and = cbit_byte_mask[a_cursor->bit];
	uint8_t l_or = l_and ^ 0xff;

	// write bit at cursor position
	a_cursor->buffer[a_cursor->byte] &= l_and;
	if (a_bit)
		a_cursor->buffer[a_cursor->byte] |= l_or;

	// advance the cursor
	if (a_cursor->bit > 0) {
		--a_cursor->bit;
	} else {
		++a_cursor->byte;
		a_cursor->bit = 7;
	}
}

/**
 * @brief Write many bits at current cursor position
 *
 * @param[in] a_cursor Pointer to cursor to use
 * @param[in] a_bits Integer containing bits to write
 * @param[in] a_count Number of bits to write
 */

void cbit_write_many(cbit_cursor_t *a_cursor, uint64_t a_bits, uint16_t a_count)
{
	// sanity check our bit count
	if (!((a_count <= 64) && (a_count > 0))) {
		fprintf(stderr, "cbit_write_many: insane bit count of %d. bit count must between 1-64.", a_count);
		exit(EXIT_FAILURE);
	}

	// shift everything over all the way to the left
	a_bits <<= (64 - a_count);

	// feed them into write_bit one at a time.
	while (a_count > 0) {
		cbit_write(a_cursor, (a_bits & 0x8000000000000000ULL) > 0);
		a_bits <<= 1;
		--a_count;
	}
}

/**
 * @brief Read single bit at cursor position
 *
 * @param[in] a_cursor Pointer to cursor to use
 */

int cbit_read(cbit_cursor_t *a_cursor)
{
	uint8_t l_mask = cbit_byte_mask[a_cursor->bit] ^ 0xff;
	uint8_t l_byte = a_cursor->buffer[a_cursor->byte] & l_mask;

	// advance the cursor
	if (a_cursor->bit > 0) {
		--a_cursor->bit;
	} else {
		++a_cursor->byte;
		a_cursor->bit = 7;
	}

	return (l_byte > 0); // true, if the bit we requested is set
}

/**
 * @brief Read many bits at current cursor position
 *
 * @param[in] a_cursor Pointer to cursor to use
 * @param[in] a_count Number of bits to read
 */

uint64_t cbit_read_many(cbit_cursor_t *a_cursor, uint16_t a_count)
{
	// sanity check our bit count
	if (!((a_count <= 64) && (a_count > 0))) {
		fprintf(stderr, "cbit_read_many: insane bit count of %d. bit count must between 1-64.", a_count);
		exit(EXIT_FAILURE);
	}

	uint64_t l_ret = 0;

	while (a_count > 0) {
		l_ret <<= 1;
		if (cbit_read(a_cursor) > 0)
			l_ret |= 0x0000000000000001ULL;
		--a_count;
	}
	return l_ret;
}

/**
 * @brief Discover how many bits wide an integer value is
 *
 * @param[in] a_val Integer value to test
 *
 * @return Number of bits that can contain the value, expressed as an unsigned short.
 */

uint16_t cbit_bit_width(uint64_t a_val)
{
	uint16_t ret = 64;

	while (ret > 0) {
		if ((a_val & 0x8000000000000000ULL) > 0)
			return ret;
		ret--;
		a_val <<= 1;
	}
	return ret;
}

