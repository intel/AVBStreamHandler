/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#include "test_wrapper/wrap_mutex.h"
#include <sys/mman.h>
#include <pthread.h>

bool wrap_pthread_condattr_init_is_active = false;
int wrap_pthread_condattr_init_result_value = 0;

extern int __real_pthread_condattr_init (pthread_condattr_t *__attr);
int __wrap_pthread_condattr_init (pthread_condattr_t *__attr)
{
  if ( wrap_pthread_condattr_init_is_active )
  {
    return wrap_pthread_condattr_init_result_value;
  }
  else
  {
    return __real_pthread_condattr_init( __attr );
  }
}


bool wrap_pthread_condattr_setpshared_is_active = false;
int wrap_pthread_condattr_setpshared_result_value = 0;

extern int __real_pthread_condattr_setpshared (pthread_condattr_t *__attr, int __pshared);
int __wrap_pthread_condattr_setpshared (pthread_condattr_t *__attr, int __pshared)
{
  if ( wrap_pthread_condattr_setpshared_is_active )
  {
    return wrap_pthread_condattr_setpshared_result_value;
  }
  else
  {
    return __real_pthread_condattr_setpshared( __attr,  __pshared);
  }
}


bool wrap_pthread_cond_init_is_active = false;
int wrap_pthread_cond_init_result_value = 0;

extern int __real_pthread_cond_init (pthread_cond_t *__restrict __cond, __const pthread_condattr_t *__restrict __cond_attr);
int __wrap_pthread_cond_init (pthread_cond_t *__restrict __cond, __const pthread_condattr_t *__restrict __cond_attr)
{
  if ( wrap_pthread_cond_init_is_active )
  {
    return wrap_pthread_cond_init_result_value;
  }
  else
  {
    return __real_pthread_cond_init( __cond,  __cond_attr);
  }
}


bool wrap_pthread_cond_wait_is_active = false;
int wrap_pthread_cond_wait_result_value = 0;

extern int __real_pthread_cond_wait (pthread_cond_t *__restrict __cond, pthread_mutex_t *__restrict __mutex);
int __wrap_pthread_cond_wait (pthread_cond_t *__restrict __cond, pthread_mutex_t *__restrict __mutex)
{
  if ( wrap_pthread_cond_wait_is_active )
  {
    return wrap_pthread_cond_wait_result_value;
  }
  else
  {
    return __real_pthread_cond_wait( __cond,  __mutex);
  }
}


