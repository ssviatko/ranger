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

carith_error_t carith_init_ctx          (carith_comp_ctx *ctx, size_t a_worksize)
{
    return CARITH_ERR_NONE;
}
