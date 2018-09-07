/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasAvbHwCaptureClockDomain.hpp
 * @brief   The definition of the IasAvbHwCaptureClockDomain class.
 * @details This is the specialized clock domain 'HwCapture'.
 * @date    2013
 */

#ifndef IASAVBHWCAPTURECLOCKDOMAIN_HPP_
#define IASAVBHWCAPTURECLOCKDOMAIN_HPP_

#include "avb_streamhandler/IasAvbTypes.hpp"
#include "avb_streamhandler/IasAvbClockDomain.hpp"
#include "avb_helper/IasThread.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#include "avb_helper/IasIRunnable.hpp"


namespace IasMediaTransportAvb {


class /*IAS_DSO_PUBLIC*/ IasAvbHwCaptureClockDomain : public IasAvbClockDomain
                                                    , private IasMediaTransportAvb::IasIRunnable
{
  public:
    /**
     *  @brief Constructor.
     */
  IasAvbHwCaptureClockDomain();


  /**
   * @brief Allocates internal resources and initializes instance.
   *
   * @returns eIasAvbProcOK on success, otherwise an error will be returned.
   */
  IasAvbProcessingResult init();


  /**
   * @brief Starts the worker thread.
   *
   * @returns eIasAvbProcOK on success, otherwise an error will be returned.
   */
  IasAvbProcessingResult start();


  /**
   * @brief Stops the worker thread.
   *
   * @returns eIasAvbProcOK on success, otherwise an error will be returned.
   */
  IasAvbProcessingResult stop();


  /**
   *  @brief Clean up all allocated resources.
   *
   *  Calling this routine turns the object into the pre-init state, i.e.
   *  init() can be called again.
   *  Also called by the destructor.
   */
  void cleanup();


    /**
     *  @brief Destructor, virtual by default.
     */
    virtual ~IasAvbHwCaptureClockDomain();

  private:
    ///
    /// Inherited from IasRunnable
    ///

    /**
     * Function called before the run function
     *
     * @returns Value indicating success or failure
     */
    virtual IasResult beforeRun();

    /**
     * The actual run function does the processing
     *
     *
     * Stay inside the function until all processing is finished or shutDown is called
     * If this call returns an error value, this error value is reported via the return value of \ref IasThread::start()
     * In case of an error, the thread still needs to be shutdown explicitly through calling \ref IasThread::stop()
     *
     * @returns Value indicating success or failure
     *
     * @note This value can be accessed through \ref IasThread::getRunThreadResult
     */
    virtual IasResult run();

    /**
     * ShutDown code
     * called when thread is going to be terminated
     *
     * Exit the \ref run function when this function is called
     *
     * @returns Value indicating success or failure
     *
     */
    virtual IasResult shutDown();

    /**
     * Function called after the run function
     *
     * @returns Value indicating success or failure
     *
     * If this call returns an error value and run() was successful, it is reported via the return value of IasThread::stop()
     */
    virtual IasResult afterRun();



  private:
    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasAvbHwCaptureClockDomain(IasAvbHwCaptureClockDomain const &other);

    /**
     * @brief Assignment operator, private unimplemented to prevent misuse.
     */
    IasAvbHwCaptureClockDomain& operator=(IasAvbHwCaptureClockDomain const &other);

    /**
     * @brief let the thread sleep
     */
    inline void sleep( uint32_t ns, uint32_t s );

    // Member variables
    std::string mInstanceName;
    volatile bool mEndThread;
    IasThread * mHwCaptureThread;
    device_t * mIgbDevice;
    double mNominal;
    uint32_t mSleep;
};


inline void IasAvbHwCaptureClockDomain::sleep( uint32_t ns, uint32_t s )
{
  // sleep some time and try again
  struct timespec req;
  struct timespec rem;
  req.tv_nsec = ns;
  req.tv_sec = s;
  nanosleep(&req, &rem);
}



} // namespace IasMediaTransportAvb

#endif /* IASAVBHWCAPTURECLOCKDOMAIN_HPP_ */
