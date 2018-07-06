/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "avb_watchdog/IasWatchdogResult.hpp"
#include "avb_helper/IasResult.hpp"

#include <sstream>
#include <string>

#define IAS_WATCHDOG_RESULT_START_VAL 1000u

namespace IasWatchdog
{

const IasWatchdogResult IasWatchdogResult::cObjectNotFound(IAS_WATCHDOG_RESULT_START_VAL + 0);
const IasWatchdogResult IasWatchdogResult::cAlreadyRegistered(IAS_WATCHDOG_RESULT_START_VAL + 1);
const IasWatchdogResult IasWatchdogResult::cWatchdogUnregistered(IAS_WATCHDOG_RESULT_START_VAL + 2);
const IasWatchdogResult IasWatchdogResult::cWatchdogNotConnected(IAS_WATCHDOG_RESULT_START_VAL + 3);
const IasWatchdogResult IasWatchdogResult::cAcquireLockFailed(IAS_WATCHDOG_RESULT_START_VAL + 4);
const IasWatchdogResult IasWatchdogResult::cIncorrectThread(IAS_WATCHDOG_RESULT_START_VAL + 5);
const IasWatchdogResult IasWatchdogResult::cTimedOut(IAS_WATCHDOG_RESULT_START_VAL + 6);
const IasWatchdogResult IasWatchdogResult::cWatchdogNotPreconfigured(IAS_WATCHDOG_RESULT_START_VAL + 7);



std::string IasWatchdogResult::toString() const
{
  std::string stringResult;
  if ((mModule == IasMediaTransportAvb::cIasResultModuleMonitoringAndLifeCycle) && (mGroup == cIasResultGroupWatchdog))
  {
    if (cObjectNotFound.mValue == mValue)
    {
      stringResult = "cObjectNotFound";
    }
    else if (cAlreadyRegistered.mValue == mValue)
    {
      stringResult = "cAlreadyRegistered";
    }
    else if (cWatchdogUnregistered.mValue == mValue)
    {
      stringResult = "cWatchdogUnregistered";
    }
    else if (cWatchdogNotConnected.mValue == mValue)
    {
      stringResult = "cWatchdogNotConnected";
    }
    else if (cAcquireLockFailed.mValue == mValue)
    {
      stringResult = "cAcquireLockFailed";
    }
    else if (cIncorrectThread.mValue == mValue)
    {
      stringResult = "cIncorrectThread";
    }
    else if (cTimedOut.mValue == mValue)
    {
      stringResult = "cTimedOut";
    }
    else if (cWatchdogNotPreconfigured.mValue == mValue)
    {
      stringResult = "cWatchdogNotPreconfigured";
    }
  }
  if (stringResult.empty())
  {
    std::stringstream resultStream;
    resultStream << mValue;
    stringResult = resultStream.str();
  }
  return stringResult;
}


}//namespace Ias



template<>
int32_t logToDlt(DltContextData &log, IasWatchdog::IasWatchdogResult const & value)
{
  return dlt_user_log_write_string(&log, value.toString().c_str());
}
