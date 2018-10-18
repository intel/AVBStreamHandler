/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbVideoCondVar.hpp
 * @brief   Simple process sharing conditional variable.
 * @details This condition variable uses futex directly, so it
 *          doesn't hang if one of the waiter process suddenly crashes.
 *
 * @date    2018
 */

#ifndef IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEOCONDVAR_HPP
#define IAS_MEDIATRANSPORT_VIDEOCOMMON_AVBVIDEOCONDVAR_HPP

#include <cstdint>

#include <sys/time.h>

namespace IasMediaTransportAvb {

/**
 * @brief A condition variable that can be shared among processes.
 *
 * It has some internal details (see CPP file for this class) that
 * makes it unsuitable for any use cases different from IasAvbVideoRingBufferShm
 * use cases.
 */
class IasAvbVideoCondVar {
  public:

    /**
     * @brief The result type for the IasAvbVideoCondVar methods
     */
    enum IasResult
    {
      eIasOk,                         //!< Operation successful
      eIasTimeout,                    //!< Timeout while waiting for condition variable
      eIasCondWaitFailed,             //!< futex wait failed
      eIasClockTimeNotAvailable,      //!< Clock time not available
      eIasCondBroadcastFailed,        //!< futex broadcast failed
    };

    /**
     * @brief Constructor
     */
    IasAvbVideoCondVar();

    /**
     * @brief Destructor
     */
    ~IasAvbVideoCondVar();

    /**
     * @brief Wait for the condition variable to be signaled for a certain time
     *
     * @param[in] time_ms timespan to wait at maximum for a condition variable
     *
     * @returns The status of the method call
     * @retval eIasOk Condition variable was signaled before timeout
     * @retval eIasTimeout Timeout while waiting for condition variable
     * @retval eIasClockTimeNotAvailable Clock time not available
     * @retval eIasCondWaitFailed Futex wait operation failed
     */
    IasResult wait(uint64_t time_ms);

    /**
     * @brief Broadcast the condition variable
     *
     * @returns The status of this call or the error that occurred during initialization
     * @retval eIasOk Condition variable was signaled
     * @retval eIasCondBroadcastFailed Futex broadcast failed
     */
    IasResult broadcast();

  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbVideoCondVar(IasAvbVideoCondVar const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbVideoCondVar& operator=(IasAvbVideoCondVar const &other);

    /**
     * @brief Wrapper for the futex system call
     */
    int futex(int *uaddr, int futex_op, int val,
            const struct timespec *timeout, int *uaddr2, int val3);

    int mFutex;  //!< The futex variable
};

}

#endif
