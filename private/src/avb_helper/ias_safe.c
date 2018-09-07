/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */
#include <stddef.h>
#include <string.h>
#include "avb_helper/ias_safe.h"

avb_safe_result avb_safe_strncpy(char * dest, size_t dest_size, const char * source, size_t source_count)
{
  size_t copy_length = source_count;

  if ((NULL == dest) || (NULL == source) || (dest_size == 0U))
  {
    return e_avb_safe_result_null;
  }

  if ((source < (dest + dest_size)) &&  (dest < (source + source_count)))
  {
    return e_avb_safe_result_overlap;
  }
  if (dest_size <= source_count)
  {
    copy_length = dest_size - 1U;
  }

  (void)strncpy(dest, source,copy_length);
  dest[copy_length] = 0;

  return e_avb_safe_result_ok;
}


avb_safe_result avb_safe_memcpy(void * dest, size_t dest_size, const void * source, size_t source_size)
{
  size_t copy_length = source_size;

  if ((NULL == dest) || (NULL == source))
  {
    return e_avb_safe_result_null;
  }

  if (((char *)source < ((char *)dest + dest_size)) &&  ((char *)dest < ((char *)source + source_size)))
  {
    return e_avb_safe_result_overlap;
  }
  if (dest_size < source_size)
  {
    copy_length = dest_size;
  }

  (void)memcpy(dest, source,copy_length);

  return e_avb_safe_result_ok;
}
