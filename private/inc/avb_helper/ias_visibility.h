/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#ifndef IAS_MEDIA_TRANSPORT_VISIBILITY_H
#define IAS_MEDIA_TRANSPORT_VISIBILITY_H
#include <stdbool.h>

#ifndef IAS_DSO_PUBLIC
#  if (__GNUC__ >= 4)  && ! defined(IAS_DSO_PUBLISH_ALL)
#    define IAS_DSO_PUBLIC   __attribute__ ((visibility ("default")))
#    define IAS_DSO_PRIVATE  __attribute__ ((visibility ("hidden")))
#  else
#    define IAS_DSO_PUBLIC
#    define IAS_DSO_PRIVATE
#  endif
#endif


#ifdef __cplusplus
# define EXTERN_C_BEGIN  extern "C" {
# define EXTERN_C_END    }
#else

/**
 * define dummy EXTERN_C_BEGIN makro for c library
 */
# define EXTERN_C_BEGIN
/**
 * define dummy EXTERN_C_END makro for c library
 */
# define EXTERN_C_END
#endif




#endif /* IAS_MEDIA_TRANSPORT_VISIBILITY_H */
