/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef IAS_MONITORING_AND_LIFECYCLE_WATCHDOG_IAS_WATCHDOG_TIMER_REGISTRATION_HPP
#define IAS_MONITORING_AND_LIFECYCLE_WATCHDOG_IAS_WATCHDOG_TIMER_REGISTRATION_HPP

#include <mutex>

#include "avb_watchdog/IasWatchdogResult.hpp"
#include <functional>
#include <memory>

namespace IasWatchdog {

class IasWatchdogTimer
{
public:
  enum class IasTimerPriority
  {
    eVeryHigh,
    eHigh,
    eDefault,
    eLow,
    eVeryLow
  };

  explicit IasWatchdogTimer(uint64_t const & timeoutInterval, std::function<void(void)> timeoutCallback, IasWatchdogTimer::IasTimerPriority const timerPriority)
    : mTimeoutInterval(timeoutInterval)
    , mTimeoutCallback(timeoutCallback)
    , mTimerPriority(timerPriority)
    {}

  IasWatchdogTimer(IasWatchdogTimer const &) = default;
  IasWatchdogTimer & operator=(const IasWatchdogTimer&) = delete;

  void setTimeoutInterval(uint64_t timeoutInterval) { mTimeoutInterval = timeoutInterval; }
  uint64_t getTimeoutInterval() const { return mTimeoutInterval; }

  IasTimerPriority getTimerPriority() const { return mTimerPriority; }
protected:
  uint64_t mTimeoutInterval;
  std::function<void(void)> mTimeoutCallback;
  IasTimerPriority mTimerPriority;
};


class IasWatchdogTimerRegistration: public std::enable_shared_from_this<IasWatchdogTimerRegistration>
{
public:
  IasWatchdogTimerRegistration(DltContext &context)
  : mDltContext(context)
  , mRegistrationMutex(/*recursive_mutex*/)
  {}

  virtual ~IasWatchdogTimerRegistration() = default;

  /**
    * \brief Notifies all listeners about a new Timeout.
    */
  IasWatchdogResult registerTimer(IasWatchdogTimer const &timer)
  {
    IasWatchdogResult result = IasWatchdogResult::cAcquireLockFailed;
    (void) timer;
    std::lock_guard<std::mutex>  guard(mRegistrationMutex);
    /*
    if ( IasMediaTransportAvb::IAS_SUCCEEDED(IasMediaTransportAvb::IasResult::cOk) )
    //if ( IasMediaTransportAvb::IAS_SUCCEEDED(guard) )
    {
      //result = registerTimerLocked(timer);
    }*/
    return result;
  }

  /**
    * \brief Notifies all listeners about the removal of a Timeout.
    */
  IasWatchdogResult unregisterTimer(IasWatchdogTimer const &timer)
  {
    IasWatchdogResult result = IasWatchdogResult::cAcquireLockFailed;
    (void) timer;
    std::lock_guard<std::mutex>  guard(mRegistrationMutex);
    /*
    if ( IasMediaTransportAvb::IAS_SUCCEEDED(IasMediaTransportAvb::IasResult::cOk))
    //if ( IasMediaTransportAvb::IAS_SUCCEEDED(guard) )
    {
      result = unregisterTimerLocked(timer);
    }*/
    return result;
  }

protected:
  //virtual IasWatchdogResult registerTimerLocked(IasWatchdogTimer const &timer) = 0;
  //virtual IasWatchdogResult unregisterTimerLocked(IasWatchdogTimer const &timer) = 0;

  DltContext mDltContext;
  std::mutex mRegistrationMutex;
};

} // namespace IasWatchdog


#endif /* IAS_MONITORING_AND_LIFECYCLE_WATCHDOG_IAS_WATCHDOG_TIMER_REGISTRATION_HPP */
