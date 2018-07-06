/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef IASWATCHDOGTHREAD_HPP
#define IASWATCHDOGTHREAD_HPP

#include "avb_helper/IasIRunnable.hpp"
#include "avb_helper/IasThread.hpp"
#include <dlt/dlt_cpp_extension.hpp>

namespace IasWatchdog {

static const std::string cClassName = "IasWatchdogThread::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

// Type definiton for shared pointers to IasWatchdogThread objects.
class IasWatchdogThread;
using IasWatchdogThreadPtr = std::shared_ptr<IasWatchdogThread>;


/**
 * @brief This class implements the worker thread that periodically 'kicks' the watchdog object.
 *
 * It inherits the IasThread class to comply with a previous implementation that used CommonAPI's MainLoop.
 */
class /*IAS_DSO_PUBLIC*/ IasWatchdogThread : public IasMediaTransportAvb::IasIRunnable
{
public:

  /**
   * @brief  Result type of the class IasWatchdogThread.
   */
  enum IasResult
  {
    eIasOk,               //!< Ok, Operation successful
    eIasInvalidParam,     //!< Invalid parameter, e.g., out of range or NULL pointer
    eIasInitFailed,       //!< Initialization of the component failed
    eIasNotInitialized,   //!< Component has not been initialized appropriately
    eIasFailed,           //!< other error
  };

  /**
   * @brief Constructor.
   *
   * @param[in] params  Initialization parameters
   */
  IasWatchdogThread()
      :mLog(new DltContext())
      ,mDltLogLevel(DLT_LOG_DEFAULT)
      ,mThreadInterval(15)
      ,mKeepRunning(true)
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);

    // register own context for DLT
    if (DLT_LOG_DEFAULT == mDltLogLevel)
    {
      DLT_REGISTER_CONTEXT(*mLog, "_ENV", "Environment");
    }
    else
    {
      DLT_REGISTER_CONTEXT_LL_TS(*mLog, "_ENV", "Environment", mDltLogLevel, DLT_TRACE_STATUS_OFF);
    }

    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  }

  /**
   * @brief Destructor.
   *
   * Class is not intended to be subclassed.
   */
  ~IasWatchdogThread()
  {
    stop();
    DLT_LOG_CXX(*mLog, DLT_LOG_VERBOSE, LOG_PREFIX);
  }

  /**
   * @brief Initialization.
   *
   * Empty to comply with previous watchdog implementation.
   */
  IasMediaTransportAvb::IasResult init()
  {
    return IasMediaTransportAvb::IasResult::cOk;
  }

  /**
   * @brief Start the watchdog worker thread.
   * @return Status of the method.
   */
  IasMediaTransportAvb::IasResult start()
  {
    return IasMediaTransportAvb::IasResult::cOk;
  }

  /**
   * @brief Intended for destructor.
   *
   * Empty to comply with previous watchdog implementation.
   *
   * @return Status of the method.
   */
  IasMediaTransportAvb::IasResult stop()
  {
    return IasMediaTransportAvb::IasResult::cOk;
  }

  /**
   * @brief Called once before starting the worker thread.
   *
   * Inherited by Ias::IasIRunnable.
   *
   * @return Status of the method.
   */
  IasMediaTransportAvb::IasResult beforeRun()
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Watchdog thread is starting...");
    return IasMediaTransportAvb::IasResult::cOk;
  }

  /**
   * @brief The main worker thread function.
   *
   * Inherited by Ias::IasIRunnable.
   *
   * @return Status of the method.
   */
  IasMediaTransportAvb::IasResult run()
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Watchdog thread is running...");

    while(mKeepRunning)
    {
      //Kick the watchdogManager periodically.
      sleep(mThreadInterval);           //TODO: Change to nano_sleep or timed sleep for higher accuracy.

      mWatchdogManager->notifyTimedOut();
    }
    return IasMediaTransportAvb::IasResult::cOk;
  }

  /**
   * @brief Called to initiate the worker thread shutdown.
   *
   * Inherited by Ias::IasIRunnable.
   *
   * @return Status of the method.
   */
  virtual  IasMediaTransportAvb::IasResult shutDown()
  {
    mKeepRunning = false;
    return IasMediaTransportAvb::IasResult::cOk;
  }

  /**
   * @brief Called once after worker thread finished executing.
   *
   * Inherited by Ias::IasIRunnable.
   *
   * @return Status of the method.
   */
  virtual  IasMediaTransportAvb::IasResult afterRun()
  {
    DLT_LOG_CXX(*mLog, DLT_LOG_INFO, LOG_PREFIX, "Watchdog thread is stopping...");
    return IasMediaTransportAvb::IasResult::cOk;
  }

  /**
   * @brief Passes the watchdog manager's pointer into this thread.
   *
   * @return NULL
   */
  void setWatchdogManager (IasSystemdWatchdogManager* wdManager)
  {
    mWatchdogManager = wdManager;
  }


private:
  /**
   * @brief Copy constructor, private unimplemented to prevent misuse.
   */
  IasWatchdogThread(IasWatchdogThread const &other);

  /**
   * @brief Assignment operator, private unimplemented to prevent misuse.
   */
  IasWatchdogThread& operator=(IasWatchdogThread const &other);

  //Member variables
  DltContext           *mLog;
  DltLogLevelType       mDltLogLevel;
  IasSystemdWatchdogManager   *mWatchdogManager;
  uint32_t              mThreadInterval;
  bool                  mKeepRunning;
};

} //namespace IasWatchdog

#endif // IasWatchdogThread_HPP
