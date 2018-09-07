/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#ifndef IAS_MEDIA_TRANSPORT_IAS_IRUNNABLE_HPP
#define IAS_MEDIA_TRANSPORT_IAS_IRUNNABLE_HPP

#include "IasResult.hpp"
#include "ias_visibility.h"

/**
 * @brief Ias
 */
namespace IasMediaTransportAvb
{

/**
 * @brief IRunnable interface for usage with the IasThread.
 */
class IAS_DSO_PUBLIC IasIRunnable
{
  public:

    /**
     * Constructor.
     */
    IasIRunnable();

    /**
     * Destructor.
     */
    virtual ~IasIRunnable();

    /**
     * Function called before the run function.
     *
     * @returns Value indicating success or failure.
     */
    virtual IasResult beforeRun() = 0;

    /**
     * The actual run function &rarr; does the processing.
     *
     * Stay inside the function until all processing is finished or shutDown is called.
     * If this call returns an error value, this error value is reported via the return value of \ref IasThread::start().
     * In case of an error, the thread still needs to be shutdown explicitly through calling \ref IasThread::stop().
     *
     * @returns Value indicating success or failure.
     *
     * @note This value can be accessed through \ref IasThread::getRunThreadResult.
     */
    virtual IasResult run() = 0;

    /**
     * ShutDown code,
     * called when thread is going to be terminated.
     *
     * Exit the \ref run function when this function is called.
     *
     * @returns Value indicating success or failure.
     *
     */
    virtual IasResult shutDown() = 0;

    /**
     * Function called after the run function.
     *
     * @returns Value indicating success or failure.
     *
     * If this call returns an error value and run() was successful, it is reported via the return value of IasThread::stop().
     */
    virtual IasResult afterRun() = 0;


};

}; //namespace Ias


#endif /* IAS_MEDIA_TRANSPORT_IAS_IRUNNABLE_HPP */
