/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file
 */

#include "avb_helper/IasThread.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"

#include <sys/prctl.h> // For setting/getting the thread's name.
#include <string.h>    // For strncpy copying the thread's name.
#include <sys/errno.h>

#define THREAD_NAME_LEN 16 // Corresponds to TASK_COMM_LEN in linux/sched.h  --> maximum size of thread name.


template<>
int32_t logToDlt(DltContextData &log, IasMediaTransportAvb::IasThreadResult const & value)
{
  return logToDlt(log, value.toString());
}

template<>
int32_t logToDlt(DltContextData & log, IasMediaTransportAvb::IasThread::IasThreadSchedulingPolicy const & value)
{
  int32_t result;
  switch (value)
  {
    case IasMediaTransportAvb::IasThread::eIasSchedulingPolicyOther:
      result = logToDlt(log, "other");
      break;
    case IasMediaTransportAvb::IasThread::eIasSchedulingPolicyFifo:
      result = logToDlt(log, "fifo");
      break;
    case IasMediaTransportAvb::IasThread::eIasSchedulingPolicyRR:
      result = logToDlt(log, "rr");
      break;
    case IasMediaTransportAvb::IasThread::eIasSchedulingPolicyBatch:
      result = logToDlt(log, "batch");
      break;
    case IasMediaTransportAvb::IasThread::eIasSchedulingPolicyIdle:
      result = logToDlt(log, "idle");
      break;
    default:
      result = logToDlt(log, static_cast<int32_t>(value));
      break;
  }
  return result;
}


namespace IasMediaTransportAvb {

IasThreadId const cIasThreadIdInvalid = -1;

/**
 * @brief Thread specific result values.
 */
const IasThreadResult IasThreadResult::cThreadAlreadyStarted(0u);
const IasThreadResult IasThreadResult::cThreadNotRunning(1u);
const IasThreadResult IasThreadResult::cCreateBarrierFailed(2u);
const IasThreadResult IasThreadResult::cInitAttributeFailed(3u);
const IasThreadResult IasThreadResult::cCreateThreadFailed(4u);
const IasThreadResult IasThreadResult::cDestroyAttributeFailed(5u);
const IasThreadResult IasThreadResult::cDestroyBarrierFailed(6u);
const IasThreadResult IasThreadResult::cWaitBarrierFailed(7u);
const IasThreadResult IasThreadResult::cJoinThreadFailed(8u);
const IasThreadResult IasThreadResult::cThreadSetNameFailed(9u);
const IasThreadResult IasThreadResult::cThreadGetNameFailed(10u);
const IasThreadResult IasThreadResult::cThreadSchedulePriorityFailed(11u);
const IasThreadResult IasThreadResult::cThreadSchedulePriorityNotPermitted(12u);
const IasThreadResult IasThreadResult::cThreadSchedulingParameterInvalid(13u);

std::string IasThreadResult::toString()const
{
  std::string stringResult;
  if ((mModule == cIasResultModuleFoundation) && (mGroup == cIasResultGroupThread))
  {
    if (cThreadAlreadyStarted.mValue == mValue)
    {
      stringResult = "cThreadAlreadyStarted";
    }
    else if (cThreadNotRunning.mValue == mValue)
    {
      stringResult = "cThreadNotRunning";
    }
    else if (cCreateBarrierFailed.mValue == mValue)
    {
      stringResult = "cCreateBarrierFailed";
    }
    else if (cInitAttributeFailed.mValue == mValue)
    {
      stringResult = "cInitAttributeFailed";
    }
    else if (cCreateThreadFailed.mValue == mValue)
    {
      stringResult = "cCreateThreadFailed";
    }
    else if (cDestroyAttributeFailed.mValue == mValue)
    {
      stringResult = "cDestroyAttributeFailed";
    }
    else if (cDestroyBarrierFailed.mValue == mValue)
    {
      stringResult = "cDestroyBarrierFailed";
    }
    else if (cWaitBarrierFailed.mValue == mValue)
    {
      stringResult = "cWaitBarrierFailed";
    }
    else if (cJoinThreadFailed.mValue == mValue)
    {
      stringResult = "cJoinThreadFailed";
    }
    else if (cThreadSetNameFailed.mValue == mValue)
    {
      stringResult = "cThreadSetNameFailed";
    }
    else if (cThreadGetNameFailed.mValue == mValue)
    {
      stringResult = "cThreadGetNameFailed";
    }
    else if (cThreadSchedulePriorityFailed.mValue == mValue)
    {
      stringResult = "cThreadSchedulePriorityFailed";
    }
    else if (cThreadSchedulePriorityNotPermitted.mValue == mValue)
    {
      stringResult = "cThreadSchedulePriorityNotPermitted";
    }
    else if (cThreadSchedulingParameterInvalid.mValue == mValue)
    {
      stringResult = "cThreadSchedulingParameterInvalid";
    }
  }

  if (stringResult.empty())
  {
    stringResult = IasResult::toString();
  }

  return stringResult;
}

IasThread::IasThread(IasIRunnable * runnableObject, std::string const &threadName, size_t stackSize)
  : mThreadName(threadName)
  , mStackSize(stackSize)
  , mAssureRunning(false)
  , mThreadState(cTHREAD_STATE_INVALID)
  , mThreadId(cIasThreadIdInvalid)
  , mRunnableObject(runnableObject)
  , mThreadStartedBarrier()
  , mThreadStartedBarrierInitialized(false)
  , mStartThreadResult(IasResult::cFailed)
  , mRunThreadResult(IasResult::cFailed)
  , mSchedulingPolicy(-1)
  , mSchedulingPriority(-1)
  , mLog(&IasAvbStreamHandlerEnvironment::getDltContext("_THX"))
{

}

IasThread::~IasThread()
{

  if ( cTHREAD_STATE_INVALID != getCurrentThreadState() )
  {
    IasMediaTransportAvb::IasThreadResult const stopResult = stop();
    if ( IAS_FAILED(stopResult) )
    {
      if ( getCurrentThreadState()&cTHREAD_STATE_IS_STOPPING_FLAG )
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, "~IasThread failed to stop thread because it looks as if someone else is currently stopping it. Waiting for stop to finish to prevent form severe errors.", stopResult, getCurrentThreadState());
        // If the thread is currently stopped by someone else, wait for the stop to be finished.
        while ( getCurrentThreadState()&cTHREAD_STATE_IS_STOPPING_FLAG )
        {
          usleep(20000);
        }
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, "~IasThread thread is now actually stopped. Continue destruction.", getCurrentThreadState());
      }
      else
      {
        DLT_LOG_CXX(*mLog, DLT_LOG_WARN, "~IasThread failed to stop thread unexpectedly. Ignoring", stopResult, getCurrentThreadState());
      }
    }
  }

  if ( mThreadStartedBarrierInitialized )
  {
    // Ensure the barrier resources are freed.
    int32_t destroyResult = pthread_barrier_destroy(&(mThreadStartedBarrier));
    if(destroyResult != 0)
    {
      DLT_LOG_CXX(*mLog, DLT_LOG_ERROR, "~IasThread pthread_barrier_destroy failed");
    }
    mThreadStartedBarrierInitialized = false;
  }
}

IasThreadResult IasThread::setSchedulingParameters(IasThreadSchedulingPolicy policy, int32_t priority)
{
  mSchedulingPriority = priority;
  IasThreadResult result = processSchedulingParameters(policy, mSchedulingPriority, mSchedulingPolicy);
  if (isRunning())
  {
    IasThreadResult commitResult = commitSchedulingParameters(mThreadId, mSchedulingPolicy, mSchedulingPriority);
    if (IAS_SUCCEEDED(result))
    {
      result = commitResult;
    }
  }
  return result;
}

IasThreadResult IasThread::getSchedulingParameters(IasThreadSchedulingPolicy & policy, int32_t & priority)
{
  return getSchedulingParameters(mThreadId, policy, priority);
}

IasThreadResult IasThread::signal(const int32_t signum)
{
  return signal(mThreadId, signum);
}

IasThreadResult IasThread::setThreadName(const IasThreadId threadId, const std::string & name)
{
  IasThreadResult result = IasThreadResult::cOk;
  std::string threadName = name;

  if (threadName.length() >= THREAD_NAME_LEN)
  {
    threadName.resize(THREAD_NAME_LEN-1);
    DLT_LOG_CXX(IasAvbStreamHandlerEnvironment::getDltContext("_THX"), DLT_LOG_WARN, "setThreadName name will be truncated to 16 characters");
  }

  if (threadName.length() == 0)
  {
    result = IasResult::cParameterInvalid;
  }
  else
  {
    if (pthread_setname_np(threadId, threadName.c_str()) < 0)
    {
      result = IasThreadResult::cThreadSetNameFailed;
    }
  }

  return result;
}

IasThreadResult IasThread::getThreadName(const IasThreadId threadId, std::string & name)
{
  char threadName[THREAD_NAME_LEN + 1];
  if (pthread_getname_np(threadId, threadName, THREAD_NAME_LEN) == 0)
  {
    name = threadName;
    return IasThreadResult::cOk;
  }
  return IasThreadResult::cThreadGetNameFailed;
}

IasThreadResult IasThread::setSchedulingParameters(const IasThreadId threadId, IasThreadSchedulingPolicy policy, int32_t priority)
{
  int32_t schedulingPolicy = 0;
  IasThreadResult result = processSchedulingParameters(policy, priority, schedulingPolicy);
  if (IAS_SUCCEEDED(result))
  {
    result = commitSchedulingParameters(threadId, schedulingPolicy, priority);
  }
  return result;
}

IasThreadResult IasThread::getSchedulingParameters(const IasThreadId threadId, IasThreadSchedulingPolicy & policy, int32_t & priority)
{
  IasThreadResult result = IasThreadResult::cOk;
  int32_t localPolicy;
  sched_param schedParam;
  uint32_t pthreadResult = pthread_getschedparam(threadId, &localPolicy, &schedParam);
  if (pthreadResult == EPERM)
  {
    return IasThreadResult::cThreadSchedulePriorityNotPermitted;
  }
  else if (pthreadResult != 0)
  {
    return IasThreadResult::cThreadSchedulePriorityFailed;
  }
  if (IAS_SUCCEEDED(result))
  {
    switch (localPolicy)
    {
      case SCHED_OTHER:
        policy = eIasSchedulingPolicyOther;
        break;

      case SCHED_FIFO:
        policy = eIasSchedulingPolicyFifo;
        break;

      case SCHED_RR:
        policy = eIasSchedulingPolicyRR;
        break;

      case SCHED_BATCH:
        policy = eIasSchedulingPolicyBatch;
        break;

      case SCHED_IDLE:
        policy = eIasSchedulingPolicyIdle;
        break;
    }
    priority = schedParam.__sched_priority;
  }
  return result;
}

IasThreadResult IasThread::signal(const IasThreadId threadId, const int32_t signum)
{
  return pthread_kill(threadId, signum) == 0 ? IasResult::cOk : IasResult::cFailed;
}

IasThreadResult IasThread::processSchedulingParameters(IasThreadSchedulingPolicy policy, int32_t & priority, int32_t & schedulingPolicy)
{
  IasThreadResult result = IasResult::cOk;

  switch (policy)
  {
    case eIasSchedulingPolicyOther:
      schedulingPolicy = SCHED_OTHER;
      break;

    case eIasSchedulingPolicyFifo:
      schedulingPolicy = SCHED_FIFO;
      break;

    case eIasSchedulingPolicyRR:
      schedulingPolicy = SCHED_RR;
      break;

    case eIasSchedulingPolicyBatch:
      schedulingPolicy = SCHED_BATCH;
      break;

    case eIasSchedulingPolicyIdle:
      schedulingPolicy = SCHED_IDLE;
      break;
  }

  // Check priority is in range.
  if (priority < sched_get_priority_min(schedulingPolicy))
  {
    priority = sched_get_priority_min(schedulingPolicy);
    result = IasThreadResult::cThreadSchedulingParameterInvalid;
  }
  else if (priority > sched_get_priority_max(schedulingPolicy))
  {
    priority = sched_get_priority_max(schedulingPolicy);
    result = IasThreadResult::cThreadSchedulingParameterInvalid;
  }

  return result;
}

IasThreadResult IasThread::commitSchedulingParameters(const IasThreadId threadId, const int32_t policy, const int32_t priority)
{
  IasThreadResult result = IasResult::cOk;
  if (policy != -1 && priority != -1)
  {
    sched_param schedParam;
    schedParam.__sched_priority = priority;
    int32_t res = pthread_setschedparam(threadId, policy, &schedParam);

    if (res == EPERM)
    {
      result = IasThreadResult::cThreadSchedulePriorityNotPermitted;
    }
    else if (res == EINVAL)
    {
      result = IasThreadResult::cThreadSchedulingParameterInvalid;
    }
    else if (res != 0)
    {
      result = IasThreadResult::cThreadSchedulePriorityFailed;
    }
  }

  return result;
}

IasThreadResult IasThread::start(bool assureRunning, IasIRunnable *runnableObject)
{
  // Automatically call stop() if thread was stared before, but the thread itself is not running any more.
  if ( getCurrentThreadState() == cTHREAD_STATE_STARTED_FLAG )
  {
    IasResult const stopResult = stop();
    if ( IAS_FAILED(stopResult) )
    {
      return IasThreadResult::cThreadAlreadyStarted;
    }
  }

  if ( !__sync_bool_compare_and_swap( &mThreadState, cTHREAD_STATE_INVALID, cTHREAD_STATE_IS_STARTING_FLAG) )
  {
    return IasThreadResult::cThreadAlreadyStarted;
  }

  // Overwrite runnable object member if a valid new one has been passed.
  if (NULL != runnableObject)
  {
    mRunnableObject = runnableObject;
  }

  IasThreadResult result = IasThreadResult::cOk;
  if(mRunnableObject == NULL)
  {
    result = IasThreadResult::cObjectInvalid;
  }

  mAssureRunning = assureRunning;

  bool destroyBarrier= false;
  if (IAS_SUCCEEDED(result) && mAssureRunning)
  {
    if ( !mThreadStartedBarrierInitialized )
    {
      if (pthread_barrier_init(&(mThreadStartedBarrier), NULL, 2) != 0)
      {
        result = IasThreadResult::cCreateBarrierFailed;
      }
      else
      {
        destroyBarrier = true;
        mThreadStartedBarrierInitialized = true;
      }
    }
  }

  IasThreadResult commitSchedulingParametersResult = IasResult::cOk;
  if (IAS_SUCCEEDED(result))
  {
    pthread_attr_t pthread_attr_default;
    bool attributeInitialized = false;

    if (pthread_attr_init(&pthread_attr_default) != 0)
    {
      result = IasThreadResult::cInitAttributeFailed;
    }
    else
    {
      attributeInitialized = true;
    }

    if (IAS_SUCCEEDED(result) && mStackSize != 0)
    {
      if (pthread_attr_setstacksize(&pthread_attr_default, mStackSize) != 0)
      {
        result = IasThreadResult::cInitAttributeFailed;
      }
    }

    if (IAS_SUCCEEDED(result))
    {
      mStartThreadResult = IasResult::cOk;
      mRunThreadResult = IasResult::cFailed;
      // Setting the thread state before creating the thread, because the run() method also accesses this variable.
      (void) __sync_or_and_fetch( &mThreadState, cTHREAD_STATE_STARTED_FLAG);
      if (pthread_create(&(mThreadId), &pthread_attr_default, run, (void*) this) != 0)
      {
        result = IasThreadResult::cCreateThreadFailed;
        // Reset the started flag.
        (void) __sync_and_and_fetch(&mThreadState, ~cTHREAD_STATE_STARTED_FLAG);
      }
      else
      {
        // Store this error code separately as we want to return it to the user,
        // however we do not want this error to prevent the creation of the thread.
        commitSchedulingParametersResult = commitSchedulingParameters(mThreadId, mSchedulingPolicy, mSchedulingPriority);
      }
    }

    if(attributeInitialized)
    {
      if (pthread_attr_destroy(&pthread_attr_default) != 0)
      {
        result = IasThreadResult::cDestroyAttributeFailed;
      }
    }
  }

  if (IAS_SUCCEEDED(result) && mAssureRunning)
  {
    int32_t barrierWaitResult = pthread_barrier_wait(&(mThreadStartedBarrier));
    if (barrierWaitResult == PTHREAD_BARRIER_SERIAL_THREAD)
    {
      if (pthread_barrier_destroy(&(mThreadStartedBarrier)) != 0)
      {
        result = IasThreadResult::cDestroyBarrierFailed;
      }

      // There was an attempt to destroy barrier.
      mThreadStartedBarrierInitialized = false;
      destroyBarrier = false;
    }
    else if (barrierWaitResult != 0)
    {
      result = IasThreadResult::cWaitBarrierFailed;
    }
    else
    {
      // Thread should destroy barrier.
      destroyBarrier = false;
    }
  }

  if (destroyBarrier)
  {
    if (pthread_barrier_destroy(&(mThreadStartedBarrier)) != 0)
    {
      result = IasThreadResult::cDestroyBarrierFailed;
    }
    mThreadStartedBarrierInitialized = false;
  }

  if (IAS_SUCCEEDED(result))
  {
    result = mStartThreadResult;
  }
  else
  {
    (void) stop();
  }

  if (IAS_SUCCEEDED(result))
  {
    result = commitSchedulingParametersResult;
  }

  (void) __sync_and_and_fetch(&mThreadState, ~cTHREAD_STATE_IS_STARTING_FLAG);

  return result;
}

IasResult IasThread::stop()
{
  if (mRunnableObject == NULL)
  {
    return IasThreadResult::cObjectInvalid;
  }

  uint8_t const currentThreadState = getCurrentThreadState();
  if ( (currentThreadState == cTHREAD_STATE_INVALID )
      || (currentThreadState& (cTHREAD_STATE_IS_STARTING_FLAG|cTHREAD_STATE_IS_STOPPING_FLAG)))
  {
    return IasThreadResult::cThreadNotRunning;
  }

  uint8_t const oldThreadState = __sync_fetch_and_or( &mThreadState, cTHREAD_STATE_IS_STOPPING_FLAG);
  // Ensure that actually we have triggered the stop. This ensures that stop cannot be run twice.
  if ( oldThreadState & cTHREAD_STATE_IS_STOPPING_FLAG )
  {
    return IasThreadResult::cThreadNotRunning;
  }
  // Ensure that when we set the stopping flag, the starting flag was not just set.
  if ( oldThreadState & cTHREAD_STATE_IS_STARTING_FLAG )
  {
    (void) __sync_and_and_fetch(&mThreadState, ~cTHREAD_STATE_IS_STOPPING_FLAG);
    return IasThreadResult::cThreadNotRunning;
  }

  IasThreadResult result = IasThreadResult::cOk;
  if (IAS_SUCCEEDED(result) && ( oldThreadState & cTHREAD_STATE_RUNNING_FLAG ) )
  {
    result = mRunnableObject->shutDown();
  }
  if (IAS_SUCCEEDED(result) && ( oldThreadState & cTHREAD_STATE_STARTED_FLAG ) )
  {
    if (pthread_join(mThreadId, NULL) != 0)
    {
      result = IasThreadResult::cJoinThreadFailed;
    }
    else
    {
      (void) __sync_and_and_fetch(&mThreadState, ~cTHREAD_STATE_STARTED_FLAG);
      AVB_ASSERT(! (mThreadState & cTHREAD_STATE_RUNNING_FLAG) );
    }
  }

  (void) __sync_and_and_fetch(&mThreadState, ~cTHREAD_STATE_IS_STOPPING_FLAG);

  if ( IAS_SUCCEEDED(result) )
  {
    AVB_ASSERT( getCurrentThreadState() == cTHREAD_STATE_INVALID );
    __sync_lock_test_and_set(&mThreadState, cTHREAD_STATE_INVALID);
  }

  return result;
}


void * IasThread::run(void * arg)
{
  IasThread * const pointerToThis = reinterpret_cast<IasThread *>(arg);

  if (pointerToThis != NULL)
  {
    (void)pointerToThis->setThreadName();

    pointerToThis->run();
  }
  return NULL;
}


void IasThread::run()
{
  mStartThreadResult = IasResult::cOk;
  mRunThreadResult = IasResult::cFailed;
  if(mRunnableObject == NULL)
  {
    if(IAS_SUCCEEDED(mStartThreadResult))
    {
      mStartThreadResult = IasResult::cObjectInvalid;
    }
  }

  if (IAS_SUCCEEDED(mStartThreadResult))
  {
    (void) __sync_or_and_fetch( &mThreadState, cTHREAD_STATE_RUNNING_FLAG);
  }

  if (mAssureRunning)
  {
    int32_t barrierWaitResult = pthread_barrier_wait(&(mThreadStartedBarrier));
    if (barrierWaitResult == PTHREAD_BARRIER_SERIAL_THREAD)
    {
      if (pthread_barrier_destroy(&(mThreadStartedBarrier)) != 0)
      {
        if(IAS_SUCCEEDED(mStartThreadResult))
        {
          mStartThreadResult = IasThreadResult::cDestroyBarrierFailed;
        }
      }
      // There was an attempt to destroy barrier.
      mThreadStartedBarrierInitialized = false;
    }
    else if (barrierWaitResult != 0)
    {
      if(IAS_SUCCEEDED(mStartThreadResult))
      {
        mStartThreadResult = IasThreadResult::cWaitBarrierFailed;
      }
    }
  }


  if (IAS_SUCCEEDED(mStartThreadResult))
  {
    mRunThreadResult = mRunnableObject->beforeRun();
    bool runWasCalled = false;
    if (IAS_SUCCEEDED(mRunThreadResult))
    {
      runWasCalled = true;
      uint8_t const currentThreadState = getCurrentThreadState();
      if ( (!(currentThreadState & cTHREAD_STATE_IS_STOPPING_FLAG))
          && (currentThreadState & cTHREAD_STATE_RUNNING_FLAG))
      {
        mRunThreadResult = mRunnableObject->run();
      }
    }
    (void) __sync_and_and_fetch(&mThreadState, ~cTHREAD_STATE_RUNNING_FLAG);
    if ( runWasCalled )
    {
      IasThreadResult const afterRunResult = mRunnableObject->afterRun();
      if(IAS_FAILED(afterRunResult))
      {
        if(IAS_SUCCEEDED(mRunThreadResult))
        {
          mRunThreadResult = afterRunResult;
        }
      }
    }
  }

  (void) __sync_and_and_fetch(&mThreadState, ~cTHREAD_STATE_RUNNING_FLAG);

}

IasThreadResult IasThread::setThreadName()
{
  return setThreadName(mThreadId, mThreadName);
}

std::string IasThread::getName() const
{
  std::string threadName;
  if (IAS_SUCCEEDED(getThreadName(mThreadId, threadName)))
  {
    return threadName;
  }
  return "";
}

}
