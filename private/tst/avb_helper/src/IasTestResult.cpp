/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file IasTestResult.cpp
 * @date 2018
 */

#include "gtest/gtest.h"

#define private public
#define protected public
#include "avb_helper/IasResult.hpp"
#include "avb_streamhandler/IasAvbStreamHandlerEnvironment.hpp"
#undef protected
#undef private


using namespace IasMediaTransportAvb;


namespace IasMediaTransportAvb
{

class IasTestResult : public ::testing::Test
{
protected:
    IasTestResult(){}

    virtual ~IasTestResult(){}

    virtual void SetUp(){}

    virtual void TearDown(){}
};

TEST_F(IasTestResult, testResultToString)
{
  IasResult result(0u, cIasResultGroupBasic, cIasResultModuleFoundation);
  std::cout << "Test result: " << result << std::endl;
  ASSERT_EQ(0, result.getValue());

  result.setErrnoValue(10u);
  ASSERT_EQ(10, result.getValue());


  IasResult res(1u, cIasResultGroupBasic, cIasResultModuleFoundation);
  ASSERT_EQ("cFailed", res.toString());
  ASSERT_FALSE(IAS_SUCCEEDED(res));
  ASSERT_TRUE(IAS_FAILED(res));

  res = IasResult::cAlreadyInitialized;
  ASSERT_EQ("cAlreadyInitialized", res.toString());

  res = IasResult::cNotInitialized;;
  ASSERT_EQ("cNotInitialized", res.toString());

  res = IasResult::cInitFailed;;
  ASSERT_EQ("cInitFailed", res.toString());

  res = IasResult::cObjectInvalid;;
  ASSERT_EQ("cObjectInvalid", res.toString());

  res = IasResult::cCleanupFailed;;
  ASSERT_EQ("cCleanupFailed", res.toString());

  res = IasResult::cParameterInvalid;;
  ASSERT_EQ("cParameterInvalid", res.toString());

  res = IasResult::cOutOfMemory;;
  ASSERT_EQ("cOutOfMemory", res.toString());

  res = IasResult::cObjectNotFound;;
  ASSERT_EQ("cObjectNotFound", res.toString());

  res = IasResult::cNotSupported;;
  ASSERT_EQ("cNotSupported", res.toString());

  res = IasResult::cTryAgain;;
  ASSERT_EQ("cTryAgain", res.toString());

  const IasResult otherIasResult(12u);
  res = otherIasResult;
  ASSERT_EQ("12", res.toString());

  IasResult res_other_gerror(0u, cIasResultGroupErrno, cIasResultModuleFoundation);
  res_other_gerror.setErrnoValue(20u);
  res_other_gerror.toString();

  IasResult res_other_nfound(0u, cIasResultGroupErrno, cIasResultGroupNetwork);
  res_other_nfound.setErrnoValue(20u);
  res_other_nfound.toString();

  IasResult res_other_ngerror(0u, cIasResultGroupNetwork, cIasResultModuleFoundation);
  res_other_ngerror.toString();
}

TEST_F(IasTestResult, testCompareResults)
{
  IasResult res_left(0u, cIasResultGroupThread, cIasResultModuleLogAndTrace);
  IasResult res_right(1u, cIasResultGroupBasic, cIasResultModuleFoundation);
  ASSERT_FALSE(res_left == res_right);
  ASSERT_TRUE(res_left != res_right);

  res_right = IasResult::cOk;
  ASSERT_FALSE(res_left == res_right);
  ASSERT_TRUE(res_left != res_right);

  IasResult res(0u, cIasResultGroupThread, cIasResultModuleFoundation);
  ASSERT_FALSE(res_left == res);
  ASSERT_TRUE(res_left != res);
}

}
