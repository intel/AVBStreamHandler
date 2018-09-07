/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
*/
/**
 * @file    IasBrd2ClockDriver.cpp
 * @brief   Reference implementation of clock driver interface for IVI-BRD2.
 * @date    2013
 */

#include "media_transport/avb_streamhandler_api/IasAvbClockDriverInterface.hpp"
#include "avb_helper/ias_visibility.h"
#include <cstring>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <cstring>
#include <errno.h>
#include <cstdio>
#include <unistd.h>

namespace {

using namespace IasMediaTransportAvb;

static const char regKeyClockDrvDevName[] = "clockdriver.config.i2cdevice";
static const char regKeyClockDrvI2CAddr[] = "clockdriver.config.i2caddr";
static const char regKeyClockDrvVerbLvl[] = "clockdriver.verbosity";


class IviBrd2ClockDriver : public IasAvbClockDriverInterface
{
  public:
    IviBrd2ClockDriver();
    virtual ~IviBrd2ClockDriver();

  private:
    //{@
    /// @brief  IasAvbClockDriverInterface implementation
    IasAvbResult init( const IasAvbRegistryQueryInterface & registry );
    void cleanup();
    void updateRelative( uint32_t driverId, double relVal );
    //@}

    //
    // helpers
    //
    bool setAddress();

    //
    // attributes
    //

    double mFrequency;
    int32_t mFd;
    uint16_t mI2cSlaveAddr;
    uint8_t mVerbosityLevel;
} theDriverObject;


IviBrd2ClockDriver::IviBrd2ClockDriver()
  : mFrequency(0.0)
  , mFd(-1)
  , mI2cSlaveAddr(0x0703u)
  , mVerbosityLevel(0)
{
}

IviBrd2ClockDriver::~IviBrd2ClockDriver()
{
  cleanup();
}

bool IviBrd2ClockDriver::setAddress()
{
  bool ret = true;
#ifndef IAS_HOST_BUILD
  if (ioctl(mFd,mI2cSlaveAddr, 0x64) < 0)
  {
      fprintf(stderr, "[IviBrd2ClockDriver] ioctl failed (%s)\n", strerror(errno));
      ret = false;
  }
#endif
  return ret;
}


IasAvbResult IviBrd2ClockDriver::init(const IasAvbRegistryQueryInterface & registry)
{
  IasAvbResult result = IasAvbResult::eIasAvbResultOk;
  std::string dev;
  const bool haveDevice = registry.queryConfigValue(regKeyClockDrvDevName, dev);
  if (!haveDevice)
  {
    fprintf(stderr, "[IviBrd2ClockDriver] error: missing I2C device name in configuration\n");
    result = IasAvbResult::eIasAvbResultErr;
  }
  else
  {
    printf("[IviBrd2ClockDriver] Opening %s\n", dev.c_str());

    uint8_t i2c_value[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0x3F, 0xFF, 0xFF, 0xFF, 0xA9, 0x0C, 0xFF, 0x3C, 0xFF, 0x0F, 0x04, 0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x40, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}; // Pll1 derived
    uint8_t i2c_addr[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38};
    uint8_t buf[2];

    (void) registry.queryConfigValue(regKeyClockDrvI2CAddr, mI2cSlaveAddr);
    (void) registry.queryConfigValue(regKeyClockDrvVerbLvl, mVerbosityLevel);

    mFd = open(dev.c_str(), O_RDWR);
    int32_t bytes_written = 0;

    if (mFd >= 0)
    {
      for (int32_t i = 0; i <= 42; i++)
      {
        if (setAddress())
        {
          buf[0]=i2c_addr[i];
          buf[1]=i2c_value[i];
          bytes_written = static_cast<int32_t>(write(mFd, &buf[0], 2));
          if(bytes_written == -1)
          {
            fprintf(stderr, "[IviBrd2ClockDriver] I2C register %02X fails to set to %02X fd:%d bytes_written:%d\n",buf[0], buf[1], mFd, bytes_written);
            result = IasAvbResult::eIasAvbResultErr;
            break;
          }
        }
      }
    }
    else
    {
      fprintf(stderr, "IviBrd2ClockDriver: failed to open %s (%s)\n", dev.c_str(), strerror(errno));
      result = IasAvbResult::eIasAvbResultErr;
    }

    mFrequency = 1.0; // 48kHz
  }

  return result;
}

void IviBrd2ClockDriver::cleanup()
{
  printf("IviBrd2ClockDriver: cleanup\n");
  if (mFd >= 0)
  {
    (void) close(mFd);
  }
}

void IviBrd2ClockDriver::updateRelative( uint32_t driverId, double relVal )
{
  (void) driverId; // not relevant when servicing only one clock

  double newFreq = mFrequency * relVal;

  uint8_t frqChangeBuf[4];

  static uint32_t cnt = 0;

  mFrequency = newFreq;

  if (newFreq > 1.015) // 1.015625
  {
    newFreq = 1.015;
  }

  if (newFreq < 0.985) // 0.984615
  {
    newFreq = 0.985;
  }


  int32_t ndiv_frac = int32_t((newFreq - 1.0) * double(8388608.0)); //AKM-PLL Steps: (1/2^23) -> 1/(1/2^23) = 8388608.0 ! -> 0,11 ppm

  frqChangeBuf[0] = 0x22; // i2c start address
  frqChangeBuf[1] = uint8_t( ndiv_frac >> 16 ) & 0x03;
  frqChangeBuf[2] = uint8_t( ndiv_frac >> 8);
  frqChangeBuf[3] = uint8_t( ndiv_frac );

  if (mFd >= 0)
  {
    int32_t bytes_written = 0;

    if(setAddress())
    {
       bytes_written = static_cast<int32_t>(write(mFd, frqChangeBuf, sizeof frqChangeBuf));
       if(bytes_written == -1)
       {
         fprintf(stderr, "IviBrd2ClockDriver: I2C register fails to set fd:%d bytes_written:%d\n",mFd,bytes_written);
       }
       else
       {
         cnt++;
         if(cnt == 20)
         {
           cnt = 0;
           if(mVerbosityLevel >= 1)
           {
             printf("IviBrd2ClockDriver: updateRelative successful fd:%d bytes_written:%d value: %x %x %x %x %lf\n",mFd,bytes_written,frqChangeBuf[0],frqChangeBuf[1],frqChangeBuf[2],frqChangeBuf[3], mFrequency);
           }
         }
       }
     }
  }
  else
  {
    fprintf(stderr, "IviBrd2ClockDriver: I2C device not open\n");
  }
}


} // end anon namespace

namespace IasMediaTransportAvb
{

// the only function directly exported by the shared library
IAS_DSO_PUBLIC IasAvbClockDriverInterface & getIasAvbClockDriverInterfaceInstance()
{
  return theDriverObject;
}

}

