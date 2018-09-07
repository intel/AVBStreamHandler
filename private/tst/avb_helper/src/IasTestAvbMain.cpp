/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/*
 * @file IasTestAvbMain.cpp
 * @date 2013
 */

#include <unistd.h>
#include "gtest/gtest.h"
#include "media_transport/avb_streamhandler_api/IasAvbStreamHandlerTypes.hpp"

#include "test_common/IasSpringVilleInfo.hpp"
#include <iostream>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

//extern Ias::Int32 verbosity;
extern std::string cIasChannelNamePolicyTest;
const std::string avbStreamPath = "./avb_streamhandler_demo";
const std::string avbConfigPath = "pluginias-media_transport-avb_configuration_example.so";

namespace IasMediaTransportAvb
{

class IasTestAvbMain : public ::testing::Test
{
protected:
  IasTestAvbMain()
  {
  }

  virtual ~IasTestAvbMain() {}

  // Sets up the test fixture.
  virtual void SetUp()
  {
    ASSERT_TRUE(fs::exists(avbStreamPath));
  }

  virtual void TearDown()
  {
  }

  std::string mCmdline;

  inline void startStreamHandler(const char* cmdLine)
  {
    mCmdline.assign(avbStreamPath);
    mCmdline.append(cmdLine);

    std::cout << "IasTestAvbMain::startStreamHandler cmd: " << mCmdline.c_str() << std::endl;
    if ( -1 == system(mCmdline.c_str()))
    {
      std::cout << "IasTestAvbMain::startStreamHandler failed" << std::endl;
    }
    sleep(1);
  }

  inline void stopStreamHandler()
  {
    mCmdline.clear();
    if ( -1 == system("killall avb_streamhandler_demo"))
    {
      std::cout << "IasTestAvbMain::stopStreamHandler failed" << std::endl;
    }
    sleep(1);
  }
};

TEST_F(IasTestAvbMain, PassParamList)
{
  char cmdline[1024];

  ASSERT_TRUE(IasSpringVilleInfo::fetchData());

  ::snprintf(cmdline, 1024," --background -c -s %s setup -t GrMrb -p MasterExample --ifname %s",
             avbConfigPath.c_str(),
             IasSpringVilleInfo::getInterfaceName()
             );

  startStreamHandler(cmdline);
  stopStreamHandler();

}

}
