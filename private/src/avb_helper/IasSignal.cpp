/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#include "avb_helper/IasSignal.hpp"
#include <errno.h>
//#include <pthread>

/**
 * @brief Namespace Ias
 */
namespace IasMediaTransportAvb {


IasSignal::IasSignal()
  : mConditionalInitialized(false)
  , mMaxNumSignalsPending(1u)
{
  mNumSignalsPending = 0u;

  pthread_condattr_t condAttr;
  mPmutex = PTHREAD_MUTEX_INITIALIZER;
  mConditional = PTHREAD_COND_INITIALIZER;

  // init the conditional.
  if (pthread_condattr_init(&condAttr) == 0)
  {
    if (pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC) == 0)
    {
      if (pthread_cond_init(&mConditional, &condAttr) == 0)
      {
        mConditionalInitialized = true;
      }
    }
  }
  else
  {
    pthread_condattr_destroy(&condAttr);
  }
}

IasSignal::~IasSignal()
{
  if (mConditionalInitialized)
  {
    if ( pthread_cond_destroy(&mConditional)!= 0 )
    {
      mConditional = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
    }
    mConditionalInitialized = false;
  }
}

bool IasSignal::signal()
{
  bool result = true;

  pthread_mutex_lock(&mPmutex);
  if (mNumSignalsPending >= mMaxNumSignalsPending)
  {
    result = false;
  }
  else
  {
    ++mNumSignalsPending;

    int32_t const signalResult = pthread_cond_signal(&mConditional);
    if(signalResult != 0)
    {
      result = false;
    }
  }
  pthread_mutex_unlock(&mPmutex);
  return result;
}


bool IasSignal::wait()
{
  bool result = true;

  pthread_mutex_lock(&mPmutex);

  while (mNumSignalsPending <= 0)
  {
    int32_t const waitResult = pthread_cond_wait(&mConditional, &mPmutex);
    if(waitResult != 0)
    {
      result = false;
    }
  }
  if(result)
  {
    if(mNumSignalsPending > 0)
    {
        --mNumSignalsPending;
    }
    else
    {
      // No signals pending.
      result = false;
    }
  }

  pthread_mutex_unlock(&mPmutex);

  return result;
}

}
