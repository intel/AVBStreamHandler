/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#include "test_wrapper/wrap_thread.h"
#include <sys/mman.h>
#include <pthread.h>

bool wrap_pthread_create_is_active = false;
int wrap_pthread_create_result_value = 0;

extern int __real_pthread_create(pthread_t *__restrict __newthread, __const pthread_attr_t *__restrict __attr,
    void *(*__start_routine)(void *), void *__restrict __arg);
int __wrap_pthread_create(pthread_t *__restrict __newthread, __const pthread_attr_t *__restrict __attr,
    void *(*__start_routine)(void *), void *__restrict __arg)
{
  if (wrap_pthread_create_is_active)
  {
    return wrap_pthread_create_result_value;
  }
  else
  {
    return __real_pthread_create(__newthread, __attr, __start_routine, __arg);
  }
}


