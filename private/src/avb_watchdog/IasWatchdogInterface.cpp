/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "avb_watchdog/IasWatchdogInterface.hpp"
#include "avb_watchdog/IasSystemdWatchdogManager.hpp"
#include <mutex>

// getpid
#include <sys/types.h>
#include <unistd.h>

// pthread_self
#include <pthread.h>

namespace IasWatchdog {

IasWatchdogInterface::IasWatchdogInterface(IasSystemdWatchdogManager& watchdogManager, DltContext& context)
  : mDltContext(context)
  , mWatchdogManager(watchdogManager)
  , mDueResetTime()
  , mDueResetTimeMutex()
{
  mTimeout = 0u;
  mProcessId = 0u;
  mThreadId = 0u;
  mName = "";
  mPreconfigured = false;
}

IasWatchdogInterface::~IasWatchdogInterface()
{
  IasWatchdogResult result = mWatchdogManager.removeWatchdog(this);
  if ( IasMediaTransportAvb::IAS_SUCCEEDED(result) )
  {
    DLT_LOG_CXX(
        mDltContext,
        DLT_LOG_WARN,
        "Watchdog desctructor called without previous call to IasIWatchdogManagerInterface::destroyWatchdog()", *this);
  }
}

IasWatchdogResult IasWatchdogInterface::registerWatchdog(uint32_t timeout, std::string watchdogName)
{
  IasWatchdogResult result = IasMediaTransportAvb::IasResult::cOk;

  if( watchdogName.empty() || (timeout == 0))
  {
    DLT_LOG_CXX(
            mDltContext,
            DLT_LOG_ERROR,
            "Failed to register watchdog, given watchdog name is empty or invalid timeout");
    return IasWatchdogResult::cParameterInvalid;
  }

  if ( IasMediaTransportAvb::IAS_SUCCEEDED(result) && mProcessId!= 0u )
  {
    DLT_LOG_CXX(
        mDltContext,
        DLT_LOG_ERROR,
        "Registering watchdog", watchdogName, " with timeout", timeout, " failed. Watchdog already registered", *this);
    result = IasWatchdogResult::cAlreadyRegistered;
  }
  else
  {
    std::lock_guard<std::mutex> mutexGuard(mDueResetTimeMutex);

    mTimeout = timeout;
    mProcessId = getpid();
    mThreadId =  static_cast<uint64_t>(pthread_self()); //(unsigned int) pthread_self();
    mName = watchdogName;
    mDueResetTime = 0u;
  }

  return result;
}

IasWatchdogResult IasWatchdogInterface::registerWatchdog()
{
  IasWatchdogResult result = IasMediaTransportAvb::IasResult::cOk;

  if (!mPreconfigured)
  {
    DLT_LOG_CXX(
        mDltContext,
        DLT_LOG_ERROR,
        "Registering preconfigured watchdog failed. Watchdog was not preconfigured with name and timeout", *this);
    result = IasWatchdogResult::cWatchdogNotPreconfigured;
  }
  else
  {
    result = registerWatchdog(mTimeout, mName);
  }

  return result;
}

IasWatchdogResult IasWatchdogInterface::unregisterWatchdog()
{
  IasWatchdogResult result = IasMediaTransportAvb::IasResult::cOk;

  if ( mProcessId == 0u )
  {
    DLT_LOG_CXX(
        mDltContext,
        DLT_LOG_ERROR,
        "Unregistering watchdog failed. Watchdog not registered"
        "current PID=", static_cast<int32_t>(getpid()), "currentThreadId=", static_cast<uint64_t>(pthread_self()));
    result = IasWatchdogResult::cWatchdogUnregistered;
  }
  else
  {
    std::lock_guard<std::mutex> mutexGuard(mDueResetTimeMutex);

    if (!mPreconfigured)
    {
      mTimeout = 0u;
      mName = "";
    }
    mProcessId = 0u;
    mThreadId = 0u;
    mDueResetTime = 0u;
  }
  return result;
}

uint64_t IasWatchdogInterface::getDueResetTime() const
{
  // Select a time that differs from zero to ensure valid timestamp if mutex lock fails.
  uint64_t dueResetTime  = 0u;
  dueResetTime = dueResetTime + 1000000; //1ms
  std::lock_guard<std::mutex> mutexGuard(mDueResetTimeMutex);

  dueResetTime = mDueResetTime;
  return dueResetTime;
}

uint64_t IasWatchdogInterface::getCurrentRawTime(){
  uint64_t result_timestamp = 0u; //monotonic clock is able to record up to 292 years before wraparound
  struct timespec time;
	if ( clock_gettime(CLOCK_MONOTONIC, &time) == 0 )
	{
	  result_timestamp = (time.tv_sec*1000000000UL) + time.tv_nsec;
  }

  return result_timestamp;
}

IasWatchdogResult IasWatchdogInterface::reset()
{
  IasWatchdogResult result = IasMediaTransportAvb::IasResult::cOk;

  const uint64_t callerId = static_cast<uint64_t>(pthread_self());
  if ( mProcessId == 0u )
  {
    DLT_LOG_CXX(
        mDltContext,
        DLT_LOG_ERROR,
        "Reset of watchdog failed. Watchdog not registered "
        "current PID=", static_cast<int32_t>(getpid()), "currentThreadId=", callerId);
    result = IasWatchdogResult::cWatchdogUnregistered;
  }
  else if (mThreadId != callerId)
  {
    DLT_LOG_CXX(
        mDltContext,
        DLT_LOG_ERROR,
        "Missmatch in thread id reset called from thread ",
        callerId, *this);
    result = IasWatchdogResult::cIncorrectThread;
  }
  else
  {
    uint64_t const currentTime = IasWatchdogInterface::getCurrentRawTime();
    if ( (currentTime < mDueResetTime) || (mDueResetTime==0u) )
    {
      std::lock_guard<std::mutex> mutexGuard(mDueResetTimeMutex);

      mDueResetTime =  currentTime + (mTimeout * 1000000); // 1 ms change to 1000000 ns for monotonic raw
      DLT_LOG_CXX(
      mDltContext,
      DLT_LOG_VERBOSE,
      "Resetting watchdog", *this);

      if( mTimeoutReported )
      {
          DLT_LOG_CXX(
              mDltContext,
              DLT_LOG_VERBOSE,
              "Resetting watchdog - Watchdog recovered", *this);
      }
      mTimeoutReported = false;
    }
    else
    {
      result = IasWatchdogResult::cTimedOut;

      if( !mTimeoutReported )
      {
        DLT_LOG_CXX(
          mDltContext,
          DLT_LOG_ERROR,
          "Reset of watchdog failed, because watchdog already timed-out. current-time=",
           currentTime/1000000,
           "due reset time=", mDueResetTime/1000000,
           *this);
      }
      mTimeoutReported = true;
    }
  }
  return result;
}

void IasWatchdogInterface::setTimeout(const uint32_t timeout)
{
  mTimeout = timeout;
  mPreconfigured = true;
}

void IasWatchdogInterface::setName(const std::string& name)
{
  mName = name;
  mPreconfigured = true;
}


} // namespace IasWatchdog


template<>
int32_t logToDlt(DltContextData &log, IasWatchdog::IasWatchdogInterface const &value)
{
  int result = 0;

  result += logToDlt(log, "Watchdog(");
  result += logToDlt(log, value.getName());
  result += logToDlt(log, value.getProcessId());
  result += logToDlt(log, value.getThreadId());
  result += logToDlt(log, value.getTimeout());
  result += logToDlt(log, ")");

  if (result != 0)
  {
    result = -1;
  }
  return result;

}
