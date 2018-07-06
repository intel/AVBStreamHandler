/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef IAS_MONITORING_AND_LIFECYCLE_WATCHDOG_IAS_WATCHDOG_RESULT_HPP
#define IAS_MONITORING_AND_LIFECYCLE_WATCHDOG_IAS_WATCHDOG_RESULT_HPP

#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"
#include "avb_helper/IasResult.hpp"
#include <dlt/dlt_cpp_extension.hpp>
#include <string>

/**
 * @brief Ias
 */
namespace IasWatchdog
{

/**
 * @brief Ias
 */
enum IasResultMonitoringAndLifecycleGroups {
  cIasResultGroupWatchdog
};

/**
 * @brief Custom result class for the Watchdog component.
 */
class /*IAS_DSO_PUBLIC*/ IasWatchdogResult : public IasMediaTransportAvb::IasResult
{
  public:
  /**
   * @brief WatchdogResult Constructor.
   * @param[in] value the value of the thread result.
   */
  explicit IasWatchdogResult(uint32_t const value)
    : IasResult(value, cIasResultGroupWatchdog, IasMediaTransportAvb::cIasResultModuleMonitoringAndLifeCycle)
  {
  }

  /**
   * @brief IasWatchdogResult Constructor creating IasWatchdogResult from IasResult.
   * @param[in] result IasResult to convert.
   */
  IasWatchdogResult(IasMediaTransportAvb::IasResult const & result)
    : IasResult(result)
  {
  }

  virtual std::string toString()const;

  static const IasWatchdogResult cObjectNotFound;          /**< Result value cObjectNotFound */
  static const IasWatchdogResult cAlreadyRegistered;       /**< Result value cAlreadyRegistered */
  static const IasWatchdogResult cWatchdogUnregistered;    /**< Result value cWatchdogUnregistered */
  static const IasWatchdogResult cWatchdogNotConnected;    /**< Result value cWatchdogNotConnected */
  static const IasWatchdogResult cAcquireLockFailed;       /**<acquire of lock failed */
  static const IasWatchdogResult cIncorrectThread;         /**<reset called by incorrect thread */
  static const IasWatchdogResult cTimedOut;                /**<reset called too late. Watchdog already timed out */
  static const IasWatchdogResult cWatchdogNotPreconfigured;/**< Result value cWatchdogNotPreconfigured */
};

}

template<>
IAS_DSO_PUBLIC int32_t logToDlt(DltContextData &log, IasWatchdog::IasWatchdogResult const & value);

#endif // IAS_MONITORING_AND_LIFECYCLE_WATCHDOG_IAS_WATCHDOG_RESULT_HPP
