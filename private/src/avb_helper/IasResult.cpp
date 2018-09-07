/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#include "avb_helper/IasResult.hpp"
#include <sstream>
#include <string.h>

namespace IasMediaTransportAvb
{

  const IasResult IasResult::cOk(0u);
  const IasResult IasResult::cFailed(1u);
  const IasResult IasResult::cAlreadyInitialized(2u);
  const IasResult IasResult::cNotInitialized(3u);
  const IasResult IasResult::cInitFailed(4u);
  const IasResult IasResult::cObjectInvalid(5u);
  const IasResult IasResult::cCleanupFailed(6u);
  const IasResult IasResult::cParameterInvalid(7u);
  const IasResult IasResult::cOutOfMemory(8u);
  const IasResult IasResult::cObjectNotFound(9u);
  const IasResult IasResult::cNotSupported(10u);
  const IasResult IasResult::cTryAgain(11u);


  std::string IasResult::toString()const
  {
    std::string stringResult;
    if ( (mModule == cIasResultModuleFoundation) && (mGroup == cIasResultGroupBasic ) )
    {
      if ( cOk.mValue == mValue )
      {
        stringResult = "cOk";
      }
      else if (cFailed.mValue == mValue )
      {
        stringResult = "cFailed";
      }
      else if (cAlreadyInitialized.mValue == mValue )
      {
        stringResult = "cAlreadyInitialized";
      }
      else if (cNotInitialized.mValue == mValue )
      {
        stringResult = "cNotInitialized";
      }
      else if (cInitFailed.mValue == mValue )
      {
        stringResult = "cInitFailed";
      }
      else if (cObjectInvalid.mValue == mValue )
      {
        stringResult = "cObjectInvalid";
      }
      else if (cCleanupFailed.mValue == mValue )
      {
        stringResult = "cCleanupFailed";
      }
      else if (cParameterInvalid.mValue == mValue )
      {
        stringResult = "cParameterInvalid";
      }
      else if (cOutOfMemory.mValue == mValue )
      {
        stringResult = "cOutOfMemory";
      }
      else if (cObjectNotFound.mValue == mValue )
      {
        stringResult = "cObjectNotFound";
      }
      else if (cNotSupported.mValue == mValue )
      {
        stringResult = "cNotSupported";
      }
      else if (cTryAgain.mValue == mValue )
      {
        stringResult = "cTryAgain";
      }
      // No else here because non-existing values are checked via stringResult.empty().
    }
    else if ( (mModule == cIasResultModuleFoundation) && (mGroup == cIasResultGroupErrno ) )
    {
      stringResult = strerror(mValue);
    }
    // No else here because non-existing values for group/module are checked via stringResult.empty().

    if ( stringResult.empty() )
    {
      std::stringstream resultStream;
      resultStream << mValue;
      stringResult = resultStream.str();
    }
    return stringResult;
  }


};


template<>
int32_t logToDlt(DltContextData &log, IasMediaTransportAvb::IasResult const & value)
{
  return logToDlt(log, value.toString());
}

