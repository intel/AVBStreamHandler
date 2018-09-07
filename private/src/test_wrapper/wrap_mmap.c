/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#include "test_wrapper/wrap_mmap.h"
#include <sys/mman.h>

bool wrap_mmap_is_active = false;
void * wrap_mmap_result_value = (void*)0;

extern void * __real_mmap (void *__addr, size_t __len, int __prot, int __flags, int __fd, __off_t __offset);
void * __wrap_mmap (void *__addr, size_t __len, int __prot, int __flags, int __fd, __off_t __offset)
{
  if ( wrap_mmap_is_active )
  {
    return wrap_mmap_result_value;
  }
  else
  {
    return __real_mmap( __addr,  __len, __prot, __flags, __fd, __offset);
  }
}



