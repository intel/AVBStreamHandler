/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#ifndef IAS_MEDIA_TRANSPORT_IAS_SIGNAL_HPP
#define IAS_MEDIA_TRANSPORT_IAS_SIGNAL_HPP

#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"
#include "ias_visibility.h"
#include <dlt_cpp_extension.hpp>
#include <mutex>

/**
 * @brief IasMediaTransportAvb
 */
namespace IasMediaTransportAvb {

  /**
   * @brief Base implementation of a signal.
   *
   * A signal is a counting semaphore.
   *
   * The internal protected counting mechanism ensures every call to signal()
   * really is addressed by a corresponding wait() call.
   *
   */
  class IAS_DSO_PUBLIC IasSignal
  {
    public:
      /**
       * Default constructor.
       */
      IasSignal();

      /**
        * Destructor.
        */
      virtual ~IasSignal();

      /**
       * Signal the signal.
       *
       * This increases the internal signalling counter and wakes up threads that called wait().
       *
       * @returns indicating success (true) or failure (false).
       */
      bool signal();


      /**
       * Wait for this object to be signalled by call of signal().
       *
       * If this object was signalled already before the wait call, no sleep is performed at all.
       * This call decreases the internal signalling counter according to the selected IasSignalDecreaseMode.
       *
       *
       * @returns indicating success (true) or failure (false).
       */
      bool wait();



      /*!
       *  @brief Copy constructor, marked as deleted because objects of this class should not be copied.
       */
      IasSignal(IasSignal const &other) = delete;

      /*!
       *  @brief Assignment operator, marked as deleted because objects of this class should not be copied.
       */
      IasSignal& operator=(IasSignal const &other) = delete;

      /*!
       *  @brief Move constructor, marked as deleted because objects of this class should not be copied.
       */
      IasSignal(IasSignal &&other) = delete;

      /*!
       *  @brief Move Assignment operator, marked as deleted because objects of this class should not be copied.
       */
      IasSignal& operator=(IasSignal &&other) = delete;

    private:
      pthread_mutex_t mPmutex;
      pthread_cond_t  mConditional;
      bool mConditionalInitialized;

      int32_t    mNumSignalsPending;
      uint16_t   mMaxNumSignalsPending;
  };

} // nemspace

#endif // IAS_MEDIA_TRANSPORT_IAS_SIGNAL_HPP
