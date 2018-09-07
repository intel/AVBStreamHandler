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
const std::string avbConfigPath = "pluginias-media_transport-avb_configuration_reference.so";

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
    system(mCmdline.c_str());
    sleep(1);
  }

  inline void stopStreamHandler()
  {
    mCmdline.clear();
    system("killall avb_streamhandler_demo");
    std::cout << "IasTestAvbMain::stopStreamHandler" << std::endl;
    sleep(1);
  }
};

TEST_F(IasTestAvbMain, PassParamList)
{
  char cmdline[1024];

  ASSERT_TRUE(IasSpringVilleInfo::fetchData());

  ::snprintf(cmdline, 1024," --background -c -s %s setup -t Fedora -p UnitTests --ifname %s",
             avbConfigPath.c_str(),
             IasSpringVilleInfo::getInterfaceName()
             );

  startStreamHandler(cmdline);
  system("killall -s SIGUSR1 avb_streamhandler_demo");
  system("killall -s SIGUSR1 avb_streamhandler_demo");
  sleep(1);
  system("killall -s SIGUSR2 avb_streamhandler_demo");
  system("killall -s SIGUSR2 avb_streamhandler_demo");
  stopStreamHandler();

  startStreamHandler(" -c -q -vov --clockdriver /lib/modules --clockdriver xxx --help -x");
  stopStreamHandler();

  startStreamHandler(" --nosetup nonSetup");
  stopStreamHandler();

  startStreamHandler(" --background");
  stopStreamHandler();

  startStreamHandler(" -vv -v -q");
  stopStreamHandler();

  startStreamHandler(" --help");
  stopStreamHandler();

  std::string noIPC(" --noipc");
  std::string sCmdline(cmdline);
  sCmdline.insert(0, noIPC);
  startStreamHandler(sCmdline.c_str());
  system("killall -s SIGUSR1 avb_streamhandler_demo");
  sleep(1);
  system("killall -s SIGUSR2 avb_streamhandler_demo");
  stopStreamHandler();

  startStreamHandler(" --verbose setup --config testkey=teststring");
  stopStreamHandler();

  startStreamHandler(" --verbose setup --config testkey=1");
  stopStreamHandler();

  startStreamHandler(" --verbose setup -s BadName");
  stopStreamHandler();

  startStreamHandler(" --verbose setup --numstreams=4");
  stopStreamHandler();

  startStreamHandler(" --verbose setup --numstreams=1");
  stopStreamHandler();

  startStreamHandler(" --verbose setup --hwcapture=101");
  stopStreamHandler();

  startStreamHandler(" -v");
  stopStreamHandler();

  startStreamHandler(" -vv");
  stopStreamHandler();

  startStreamHandler(" -vvv");
  stopStreamHandler();

  startStreamHandler(" -vvvv");
  stopStreamHandler();

  startStreamHandler(" -vvvvv");
  stopStreamHandler();

  startStreamHandler(" --default");
  stopStreamHandler();

  startStreamHandler(" -d");
  stopStreamHandler();

  startStreamHandler(" -I testInstance");
  stopStreamHandler();

  startStreamHandler(" -I");
  stopStreamHandler();

  startStreamHandler(" -s");
  stopStreamHandler();

}

}
