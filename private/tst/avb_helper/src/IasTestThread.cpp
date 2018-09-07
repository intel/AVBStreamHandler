/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file IasTestThread.cpp
 * @date 2018
 */

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_helper/IasThread.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#undef protected
#undef private


using namespace IasMediaTransportAvb;


namespace IasMediaTransportAvb
{

class IasTestThread : public ::testing::Test
{
protected:
    IasTestThread(){}

    virtual ~IasTestThread(){}

    virtual void SetUp(){}

    virtual void TearDown(){}
};

TEST_F(IasTestThread, testResultToString_cThreadAlreadyStarted)
{
    IasResult result(0u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cThreadAlreadyStarted", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cThreadNotRunning)
{
    IasResult result(1u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cThreadNotRunning", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cCreateBarrierFailed)
{
    IasResult result(2u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cCreateBarrierFailed", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cInitAttributeFailed)
{
    IasResult result(3u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cInitAttributeFailed", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cCreateThreadFailed)
{
    IasResult result(4u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cCreateThreadFailed", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cDestroyAttributeFailed)
{
    IasResult result(5u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cDestroyAttributeFailed", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cDestroyBarrierFailed)
{
    IasResult result(6u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cDestroyBarrierFailed", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cWaitBarrierFailed)
{
    IasResult result(7u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cWaitBarrierFailed", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cJoinThreadFailed)
{
    IasResult result(8u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cJoinThreadFailed", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cThreadSetNameFailed)
{
    IasResult result(9u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cThreadSetNameFailed", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cThreadGetNameFailed)
{
    IasResult result(10u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cThreadGetNameFailed", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cThreadSchedulePriorityFailed)
{
    IasResult result(11u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cThreadSchedulePriorityFailed", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cThreadSchedulePriorityNotPermitted)
{
    IasResult result(12u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cThreadSchedulePriorityNotPermitted", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_cThreadSchedulingParameterInvalid)
{
    IasResult result(13u, cIasResultGroupThread, cIasResultModuleFoundation);
    IasThreadResult threadResult(result);
    ASSERT_EQ("cThreadSchedulingParameterInvalid", threadResult.toString());
}

TEST_F(IasTestThread, testResultToString_notcIasResultModuleFoundation)
{
    IasResult result(13u, cIasResultGroupThread, cIasResultGroupNetwork);
    IasThreadResult threadResult(result);
    ASSERT_EQ("13", threadResult.toString());
}

TEST_F(IasTestThread, stopNullThread)
{
    std::string threadName;
    IasThread thread(0, threadName, 0u);
    ASSERT_EQ(IasThreadResult::cObjectInvalid, thread.stop());
}

TEST_F(IasTestThread, setSchedulingParameters)
{
    std::string threadName;
    IasThread thread(0, threadName, 0u);
    ASSERT_EQ(IasThreadResult::cObjectInvalid, thread.stop());

    thread.setSchedulingParameters(IasMediaTransportAvb::IasThread::eIasSchedulingPolicyOther, 1u);
    thread.setSchedulingParameters(thread.mThreadId, IasMediaTransportAvb::IasThread::eIasSchedulingPolicyOther, 1u);
}

TEST_F(IasTestThread, logToDLT)
{
    IasThreadResult result = IasThreadResult::cThreadAlreadyStarted;
    DltContextData log;
    logToDlt(log, IasResult::cOk);
    const IasThreadResult res = result;
    logToDlt(log, res);

    logToDlt(log, IasThread::IasThreadSchedulingPolicy::eIasSchedulingPolicyFifo);
    logToDlt(log, IasThread::IasThreadSchedulingPolicy::eIasSchedulingPolicyIdle);
    logToDlt(log, IasThread::IasThreadSchedulingPolicy::eIasSchedulingPolicyOther);
    logToDlt(log, IasThread::IasThreadSchedulingPolicy::eIasSchedulingPolicyRR);
    logToDlt(log, IasThread::IasThreadSchedulingPolicy::eIasSchedulingPolicyBatch);
    logToDlt(log, IasThread::IasThreadSchedulingPolicy::eIasSchedulingPolicyIdle);
}


}
