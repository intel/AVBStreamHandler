/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#ifndef IAS_MEDIATRANSPORT_TEST_WRAPPER_WRAP_CONDVAR_H
#define IAS_MEDIATRANSPORT_TEST_WRAPPER_WRAP_CONDVAR_H

#include "avb_helper/ias_visibility.h"

EXTERN_C_BEGIN

extern bool wrap_pthread_condattr_init_is_active;
extern int wrap_pthread_condattr_init_result_value;

extern bool wrap_pthread_condattr_setpshared_is_active;
extern int wrap_pthread_condattr_setpshared_result_value;

extern bool wrap_pthread_cond_init_is_active;
extern int wrap_pthread_cond_init_result_value;

extern bool wrap_pthread_cond_wait_is_active;
extern int wrap_pthread_cond_wait_result_value;

EXTERN_C_END


#endif // IAS_MEDIATRANSPORT_TEST_WRAPPER_WRAP_CONDVAR_H
