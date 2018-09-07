/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#ifndef IAS_MEDIATRANSPORT_TEST_WRAPPER_WRAP_THREAD_H
#define IAS_MEDIATRANSPORT_TEST_WRAPPER_WRAP_THREAD_H

#include "avb_helper/ias_visibility.h"

EXTERN_C_BEGIN

extern bool wrap_pthread_create_is_active;
extern int wrap_pthread_create_result_value;

EXTERN_C_END


#endif // IAS_MEDIATRANSPORT_TEST_WRAPPER_WRAP_THREAD_H
