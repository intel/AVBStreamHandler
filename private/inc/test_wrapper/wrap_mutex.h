/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#ifndef IAS_MEDIATRANSPORT_TEST_WRAPPER_WRAP_MUTEX_H
#define IAS_MEDIATRANSPORT_TEST_WRAPPER_WRAP_MUTEX_H

#include "avb_helper/ias_visibility.h"

EXTERN_C_BEGIN

extern bool wrap_pthread_mutexattr_init_is_active;
extern int wrap_pthread_mutexattr_init_result_value;

extern bool wrap_pthread_mutexattr_setpshared_is_active;
extern int wrap_pthread_mutexattr_setpshared_result_value;

extern bool wrap_pthread_mutex_init_is_active;
extern int wrap_pthread_mutex_init_result_value;

extern bool wrap_pthread_mutex_lock_is_active;
extern int wrap_pthread_mutex_lock_result_value;

extern bool wrap_pthread_mutex_unlock_is_active;
extern int wrap_pthread_mutex_unlock_result_value;

EXTERN_C_END


#endif // IAS_MEDIATRANSPORT_TEST_WRAPPER_WRAP_MUTEX_H
