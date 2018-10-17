/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/

#include "avb_video_common/IasAvbVideoLog.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"

#include <dlt/dlt_cpp_extension.hpp>

namespace IasMediaTransportAvb {

static const std::string cClassName = "IasAvbVideoLog::";
#define LOG_PREFIX cClassName + __func__ + "(" + std::to_string(__LINE__) + "):"

DltContext *IasAvbVideoLog::mDltCtxDummy = NULL;
DltContext *IasAvbVideoLog::mDefaultContext = NULL;

DltContext &IasAvbVideoLog::getDltContext(const std::string &dltContextName)
{

  if (IasAvbStreamHandlerEnvironment::getDltContext)
  {
    return IasAvbStreamHandlerEnvironment::getDltContext(dltContextName);
  }

  if (mDefaultContext != nullptr)
  {
    return *mDefaultContext;
  }

  if (mDltCtxDummy == nullptr)
  {
    mDltCtxDummy = new DltContext();
    DLT_REGISTER_CONTEXT_LL_TS(*mDltCtxDummy, "_DMY", "context not found, creating dummy one", DLT_LOG_INFO,
                                 DLT_TRACE_STATUS_OFF);
    DLT_LOG_CXX(*mDltCtxDummy, DLT_LOG_WARN, LOG_PREFIX, "Context", dltContextName, "not found, creating dummy one");
  }

  return *mDltCtxDummy;
}

void IasAvbVideoLog::setDltContext(DltContext *dltContext)
{
  if (mDefaultContext != nullptr)
  {
    DLT_LOG_CXX(*mDefaultContext, DLT_LOG_WARN, LOG_PREFIX, "Replacing AVB Video Bridge default log context");
  }

  mDefaultContext = dltContext;

  if (mDefaultContext != nullptr)
  {
    DLT_LOG_CXX(*mDefaultContext, DLT_LOG_WARN, LOG_PREFIX, "Set new AVB Video Bridge default log context");
  }
}

}
