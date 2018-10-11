/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "avb_watchdog/IasSystemdWatchdogManager.hpp"
#include "avb_watchdog/IasWatchdogInterface.hpp"
#include "avb_watchdog/IasWatchdogResult.hpp"

#include <mutex>

#include <dlt/dlt_cpp_extension.hpp>
#include <stdlib.h>
#include <systemd/sd-daemon.h>
#include <sstream>

#include <algorithm>

namespace IasWatchdog {

static const uint64_t minimumWatchdogTimeoutValueUSec = 100000;

/*
 *  Constant used for calculating watchdog timeout value.
 *  Convert to milliseconds (1/1000) and call sd_notify twice per interval to be on the safe side (1/2).
 */
static const uint64_t watchdogUSec2TimespanDivisor = 2000;


IasSystemdWatchdogManager::IasSystemdWatchdogManager(DltContext& context)
  : mDltContext(context)
  , mWatchdogTimerRegistration(nullptr)
  , mResetTimespan()
  , mWatchdogTimer(0LL, std::bind(std::mem_fn(&IasSystemdWatchdogManager::notifyTimedOut),this), IasWatchdogTimer::IasTimerPriority::eVeryHigh)
  , mWatchdogInterfacesMutex(/*std::recursive_mutex.recursive_mutex()*/)
{
}

IasSystemdWatchdogManager::~IasSystemdWatchdogManager()
{
  IasSystemdWatchdogManager::cleanup();
}

void IasSystemdWatchdogManager::notifyTimedOut()
{
  uint64_t currentResetTimespan = mResetTimespan;
  if ( allWatchdogInterfacesValid() )
  {
    int32_t const sd_notify_result = sd_notify(0, "WATCHDOG=1");
    if ( sd_notify_result < 0 )
    {
      currentResetTimespan = uint64_t((mResetTimespan/1000000)/2); //convert rawtime to ms then divide 2
      DLT_LOG_CXX(mDltContext, DLT_LOG_ERROR, "IasSystemdWatchdogManager failed to call sd_notify() to reset watchdog", sd_notify_result, "Retry soon", currentResetTimespan);
    }
    else if (sd_notify_result == 0)
    {
      DLT_LOG_CXX(mDltContext, DLT_LOG_WARN, "IasSystemdWatchdogManager cannot perform sd_notify() socket to systemd not available. Disable checking.");
      currentResetTimespan = 0LL;
    }
    else
    {
      DLT_LOG_CXX(mDltContext, DLT_LOG_INFO, "IasSystemdWatchdogManager successfully called sd_notify()");
    }
  }
  else
  {
    DLT_LOG_CXX(mDltContext, DLT_LOG_ERROR, "IasSystemdWatchdogManager cannot call sd_notify() because some watchdog interfaces timed out.");
  }
}

IasWatchdogResult IasSystemdWatchdogManager::init(std::shared_ptr<IasWatchdogTimerRegistration> watchdogTimerRegistration)
{
  if(watchdogTimerRegistration == nullptr)
  {
    return IasMediaTransportAvb::IasResult::cParameterInvalid;
  }

  mWatchdogTimerRegistration = watchdogTimerRegistration;

  IasWatchdogResult result = IasMediaTransportAvb::IasResult::cOk;

  char const * const envStr = getenv("WATCHDOG_USEC");
  if (envStr != nullptr)
  {
    uint64_t intValue = 0;
    std::stringstream stream;
    stream << envStr;
    stream >> intValue;
    if ( stream.bad() )
    {
      DLT_LOG_CXX(mDltContext, DLT_LOG_ERROR, "IasSystemdWatchdogManager failed to convert WATCHDOG_USEC environment variable to integer value", envStr);
      result = IasMediaTransportAvb::IasResult::cInitFailed;
    }
    else if (intValue < minimumWatchdogTimeoutValueUSec)
    {
      DLT_LOG_CXX(mDltContext, DLT_LOG_ERROR, "IasSystemdWatchdogManager environment variable WATCHDOG_USEC too small. Values below 100000 (which are 100ms) are not acceptable.", envStr, intValue);
      result = IasMediaTransportAvb::IasResult::cInitFailed;
    }
    else
    {
      mResetTimespan = uint64_t( intValue/watchdogUSec2TimespanDivisor );
    }
  }
  else
  {
    DLT_LOG_CXX(mDltContext, DLT_LOG_WARN, "IasSystemdWatchdogManager no watchdog configuration found in env 'WATCHDOG_USEC'. Disable checking.");
  }

  if ( result.toString() == IasMediaTransportAvb::IasResult::cOk.toString() && ( mResetTimespan > 0LL) )  //watchdog result type not compatible with IAS_SUCCEED
  //if ( IasMediaTransportAvb::IAS_SUCCEEDED(result) && ( mResetTimespan > 0LL) )
  {
    DLT_LOG_CXX(mDltContext, DLT_LOG_INFO, "IasSystemdWatchdogManager successfully initialized watchdog timer with reset interval (ms)", mResetTimespan);
    // Immediately trigger a timeout.
    notifyTimedOut();
  }
  return result;
}

IasWatchdogResult IasSystemdWatchdogManager::cleanup()
{
  IasWatchdogResult result = IasMediaTransportAvb::IasResult::cOk;

  mWatchdogTimerRegistration=nullptr;
  return result;
}

IasWatchdogInterface* IasSystemdWatchdogManager::createWatchdog()
{
  IasWatchdogInterface * watchdogInterface = nullptr;
  std::lock_guard<std::recursive_mutex>  mutexGuard(mWatchdogInterfacesMutex);

  watchdogInterface = new IasWatchdogInterface(*this, mDltContext);

  if ( watchdogInterface )
  {
    mWatchdogInterfaces.push_back(watchdogInterface);
  }

  return watchdogInterface;
}

IasWatchdogInterface* IasSystemdWatchdogManager::createWatchdog(uint32_t timeout, std::string watchdogName)
{
  IasWatchdogInterface * watchdogInterface = nullptr;

  std::lock_guard<std::recursive_mutex> mutexGuard(mWatchdogInterfacesMutex);

    watchdogInterface = new IasWatchdogInterface(*this, mDltContext);
    if (watchdogInterface)
    {
      watchdogInterface->setTimeout(timeout);
      watchdogInterface->setName(watchdogName);
      mWatchdogInterfaces.push_back(watchdogInterface);
    }

    return watchdogInterface;
}

IasWatchdogResult IasSystemdWatchdogManager::destroyWatchdog(IasWatchdogInterface* watchdog)
{
  if ( watchdog==nullptr )
  {
    return IasMediaTransportAvb::IasResult::cParameterInvalid;
  }

  IasWatchdogResult result = IasMediaTransportAvb::IasResult::cOk;

  IasWatchdogInterface * watchdogInterface = dynamic_cast<IasWatchdogInterface*>(watchdog);
  if ( watchdogInterface == nullptr )
  {
    DLT_LOG_CXX(mDltContext, DLT_LOG_ERROR, "IasSystemdWatchdogManager::destroyWatchdog() given watchdog interface seems not to be created by this manager", reinterpret_cast<int64_t>(watchdog));
    result = IasMediaTransportAvb::IasResult::cParameterInvalid;
  }
  else
  {
    result = removeWatchdogInternal(watchdogInterface, IasWatchdogRemoveMode::eRemoveAndDestroy);
  }
  return result;
}

IasWatchdogResult IasSystemdWatchdogManager::removeWatchdog(IasWatchdogInterface* watchdogInterface)
{
  return removeWatchdogInternal(watchdogInterface, IasWatchdogRemoveMode::eRemoveOnly);
}

IasWatchdogResult IasSystemdWatchdogManager::removeWatchdogInternal(IasWatchdogInterface *watchdogInterface, IasSystemdWatchdogManager::IasWatchdogRemoveMode const watchdogRemoveMode)
{
  if ( watchdogInterface == nullptr )
  {
    return IasMediaTransportAvb::IasResult::cParameterInvalid;
  }
  IasWatchdogResult result = IasMediaTransportAvb::IasResult::cOk;

  std::lock_guard<std::recursive_mutex> mutexGuard(mWatchdogInterfacesMutex);

    auto searchResult = std::find(mWatchdogInterfaces.begin(), mWatchdogInterfaces.end(), watchdogInterface);
    if ( searchResult != mWatchdogInterfaces.end() )
    {
      mWatchdogInterfaces.erase(searchResult);
      // Only delete if not called from within destructor.
      if ( watchdogRemoveMode != IasWatchdogRemoveMode::eRemoveOnly)
      {
        delete watchdogInterface;
      }
    }
    else
    {
      if ( watchdogRemoveMode != IasWatchdogRemoveMode::eRemoveOnly )
      {
        DLT_LOG_CXX(mDltContext, DLT_LOG_ERROR, "IasSystemdWatchdogManager::destroyWatchdog() given watchdog interface not found in list of created interfaces", *watchdogInterface);
      }
      result = IasMediaTransportAvb::IasResult::cObjectNotFound;
    }

  return result;
}

uint64_t IasSystemdWatchdogManager::getCurrentRawTime() const
{
  uint64_t result_timestamp = 0u; //uint64_t clock is able to record up to 292 years of monotonic raw time before wraparound
  struct timespec time;
	if ( clock_gettime(CLOCK_MONOTONIC, &time) == 0 )
	{
	  result_timestamp = (time.tv_sec*1000000000UL) + time.tv_nsec;
  }

  return result_timestamp;
}

bool IasSystemdWatchdogManager::allWatchdogInterfacesValid() const
{
  bool allValid = true;
  std::lock_guard<std::recursive_mutex> mutexGuard(mWatchdogInterfacesMutex);

    uint64_t const now = IasSystemdWatchdogManager::getCurrentRawTime();
    for ( auto watchdogInterface : mWatchdogInterfaces )
    {
	    uint64_t const dueResetTime = watchdogInterface->getDueResetTime();
      //DLT_LOG_CXX(mDltContext, DLT_LOG_INFO, "IasSystemdWatchdogManager::allWatchdogInterfacesValid() found timed out watchdog with dueResetTime:", dueResetTime, " now:", now);

      if ( ( dueResetTime != 0u ) && (dueResetTime <= now) )
      {
        allValid = false;
        DLT_LOG_CXX(mDltContext, DLT_LOG_ERROR, "IasSystemdWatchdogManager::allWatchdogInterfacesValid() found timedout watchdog", *watchdogInterface);
      }
      else if ( dueResetTime == 0u  )
      {
        /*This happens if reset was not called after first initialization of a watchdog interface*/
        allValid = false;
        DLT_LOG_CXX(mDltContext, DLT_LOG_ERROR, "IasSystemdWatchdogManager::allWatchdogInterfacesValid() found un-initialized watchdog timeout", *watchdogInterface);
      }
    }

  return allValid;
}

} // namespace IasWatchdog
