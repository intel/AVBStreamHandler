/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#ifndef IAS_MEDIA_TRANSPORT_IAS_DEBUG_H
#define IAS_MEDIA_TRANSPORT_IAS_DEBUG_H

#include <csignal>
#include <iostream>

/**
 * Macro to set a breakpoint in the code.
 */
#define EMBED_BREAKPOINT raise(SIGINT);

/**
 * Klocwork always needs to see the full definition of the
 * AVB_ASSERT macro to avoid false positives.
 */
#if (!defined(DEBUG)) && (!defined(__KLOCWORK__))
/**
 * Dummy define AVB_ASSERT for release code.
 */
  #define AVB_ASSERT(x)
#else
  /**
   * Definition of an assert macro.
   */
  #define AVB_ASSERT(x) \
  if (! (x)) \
  { \
  std::cout << "ERROR!! Assert " << #x << " failed\n"; \
  std::cout << " on line " << __LINE__  << "\n"; \
  std::cout << " in file " << __FILE__ << "\n";  \
  EMBED_BREAKPOINT \
  }
#endif

#endif // IAS_MEDIA_TRANSPORT_IAS_DEBUG_H
