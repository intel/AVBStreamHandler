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

bool wrap_pthread_mutexattr_init_is_active = false;
int wrap_pthread_mutexattr_init_result_value = 0;

extern int __real_pthread_mutexattr_init (pthread_mutexattr_t *__attr);
int __wrap_pthread_mutexattr_init (pthread_mutexattr_t *__attr)
{
  if ( wrap_pthread_mutexattr_init_is_active )
  {
    return wrap_pthread_mutexattr_init_result_value;
  }
  else
  {
    return __real_pthread_mutexattr_init( __attr );
  }
}


bool wrap_pthread_mutexattr_setpshared_is_active = false;
int wrap_pthread_mutexattr_setpshared_result_value = 0;

extern int __real_pthread_mutexattr_setpshared (pthread_mutexattr_t *__attr, int __pshared);
int __wrap_pthread_mutexattr_setpshared (pthread_mutexattr_t *__attr, int __pshared)
{
  if ( wrap_pthread_mutexattr_setpshared_is_active )
  {
    return wrap_pthread_mutexattr_setpshared_result_value;
  }
  else
  {
    return __real_pthread_mutexattr_setpshared( __attr,  __pshared);
  }
}


bool wrap_pthread_mutex_init_is_active = false;
int wrap_pthread_mutex_init_result_value = 0;

extern int __real_pthread_mutex_init (pthread_mutex_t *__mutex, __const pthread_mutexattr_t *__mutexattr);
int __wrap_pthread_mutex_init (pthread_mutex_t *__mutex, __const pthread_mutexattr_t *__mutexattr)
{
  if ( wrap_pthread_mutex_init_is_active )
  {
    return wrap_pthread_mutex_init_result_value;
  }
  else
  {
    return __real_pthread_mutex_init( __mutex,  __mutexattr);
  }
}


bool wrap_pthread_mutex_lock_is_active = false;
int wrap_pthread_mutex_lock_result_value = 0;

extern int __real_pthread_mutex_lock (pthread_mutex_t *__mutex);
int __wrap_pthread_mutex_lock (pthread_mutex_t *__mutex)
{
  if ( wrap_pthread_mutex_lock_is_active )
  {
    return wrap_pthread_mutex_lock_result_value;
  }
  else
  {
    return __real_pthread_mutex_lock( __mutex);
  }
}


bool wrap_pthread_mutex_unlock_is_active = false;
int wrap_pthread_mutex_unlock_result_value = 0;

extern int __real_pthread_mutex_unlock (pthread_mutex_t *__mutex);
int __wrap_pthread_mutex_unlock (pthread_mutex_t *__mutex)
{
  if ( wrap_pthread_mutex_unlock_is_active )
  {
    return wrap_pthread_mutex_unlock_result_value;
  }
  else
  {
    return __real_pthread_mutex_unlock( __mutex);
  }
}


