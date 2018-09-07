/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#ifndef IAS_MEDIA_TRANSPORT_IAS_SAFE_H
#define IAS_MEDIA_TRANSPORT_IAS_SAFE_H

#include "ias_visibility.h"

EXTERN_C_BEGIN

#ifdef __GNUC__
#define WARN_UNUSED __attribute__((warn_unused_result))
#else
#define WARN_UNUSED
#endif

typedef enum avb_safe_result
{
  e_avb_safe_result_ok,
  e_avb_safe_result_null, //< one of the parameters is NULL
  e_avb_safe_result_overlap, //< memory overlap of input and output memory
} avb_safe_result;

/**
 * @brief safer string copy
 *
 * copies a string with the given length into given memory with length.
 * This call checks for NULL-pointers and overlap of the strings.
 * The destination string will always be NULL terminated.
 *
 * @return e_avb_safe_result_null when source or dest is NULL
 * @return e_avb_safe_result_overlap when the strings overlap
 * @return e_ias_safe_ok in all other cases
 *
 */
IAS_DSO_PUBLIC avb_safe_result WARN_UNUSED avb_safe_strncpy(char * dest, size_t dest_size, const char * source, size_t source_count);

/**
 * @brief safer memory copy
 *
 * copies a memory range with the given length into given memory with length.
 * This call checks for NULL-pointers and overlap of the memory areas.
 *
 * @return e_avb_safe_result_null when source or dest is NULL
 * @return e_avb_safe_result_overlap when the memory ranges overlap
 * @return e_ias_safe_ok in all other cases
 *
 */
IAS_DSO_PUBLIC avb_safe_result WARN_UNUSED avb_safe_memcpy(void * dest, size_t dest_size, const void * source, size_t source_size);


EXTERN_C_END

#endif /* IAS_MEDIA_TRANSPORT_IAS_SAFE_H */
